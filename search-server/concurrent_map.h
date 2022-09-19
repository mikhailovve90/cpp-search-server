#pragma once
#include <algorithm>
#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include "log_duration.h"
#include <future>
#include <type_traits>
#include <future>
#include <iterator>
#include <type_traits>
#include <execution>

using namespace std::string_literals;
template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
       Access(std::vector<std::mutex>& mutex_v, size_t group, uint64_t ukey, std::vector<std::map<Key, Value>>& map_in)
       : guard(mutex_v[group]), ref_to_value(map_in[group][ukey]) {}


       std::lock_guard<std::mutex> guard;
       Value& ref_to_value;

    };

    explicit ConcurrentMap(size_t bucket_count): vector_maps_(bucket_count), mutexs_(bucket_count){
    };

    Access operator[](const Key& key){
      std::atomic<uint64_t> ukey = static_cast<uint64_t>(key);
      size_t size = vector_maps_.size();
      size_t group = (vector_maps_.size() == 0 ? 0 : ukey % size);
      while(group >= size) group = ukey % size;


      return Access(mutexs_, group, ukey, vector_maps_);
    }

    void erase(const Key& key){
      std::atomic<uint64_t> ukey = static_cast<uint64_t>(key);
      size_t size = vector_maps_.size();
      size_t group = (vector_maps_.size() == 0 ? 0 : ukey % size);
      while(group >= size) group = ukey % size;

      std::lock_guard<std::mutex> lock(mutexs_[group]);
      auto erase_elem = vector_maps_[group].find(key);
      if(erase_elem != vector_maps_[group].end())
        vector_maps_[group].erase(erase_elem);
    }


    std::map<Key, Value> BuildOrdinaryMap(){
      std::map<Key, Value> all_map;
      for(size_t i = 0; i < mutexs_.size(); ++i) {
        std::lock_guard<std::mutex> lock(mutexs_[i]);
        all_map.insert(vector_maps_[i].begin(), vector_maps_[i].end());
      }
      return all_map;
    };

private:
    std::vector<std::map<Key, Value>> vector_maps_;
    std::vector<std::mutex> mutexs_;
};

using namespace std;
// Взято авторское решение из яндекс задания про параллелизм фор ича
template <typename ExecutionPolicy, typename ForwardRange, typename Function>
void ForEach(const ExecutionPolicy& policy, ForwardRange& range, Function function) {
    if constexpr (
        is_same_v<ExecutionPolicy, execution::sequenced_policy>
        || is_same_v<typename iterator_traits<typename ForwardRange::iterator>::iterator_category,
                     random_access_iterator_tag>
        ) {
        for_each(policy, range.begin(), range.end(), function);

    } else {
        static constexpr int PART_COUNT = 10;
        const auto part_length = size(range) / PART_COUNT;
        auto part_begin = range.begin();
        auto part_end = next(part_begin, part_length);

        vector<future<void>> futures;
        for (int i = 0;
             i < PART_COUNT;
             ++i,
                 part_begin = part_end,
                 part_end = (i == PART_COUNT - 1
                                 ? range.end()
                                 : next(part_begin, part_length))
             ) {
            futures.push_back(async([function, part_begin, part_end] {
                for_each(part_begin, part_end, function);
            }));
        }
    }
}

template <typename ForwardRange, typename Function>
void ForEach(ForwardRange& range, Function function) {
    ForEach(execution::seq, range, function);
}
