#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer &s_s){
  auto end_ = s_s.end();
  std::set<int> doc_on_delete;
  //Множество где ключЁм является множество
  std::set<std::set<std::string_view >> result;
  //Ключ на данной итерации
  std::set<std::string_view> key;

  for(auto j = s_s.begin(); j != end_; ++j){
    auto id = *j;
    auto string_map = s_s.GetWordFrequencies(id);
    //Перекладываю мэп в множество являющееся ключём
    for(auto [str, _] : string_map) key.insert(str);
    if(result.count(key)) doc_on_delete.insert(id);
    else result.insert(key);
    key.clear();
  }

  for(auto id : doc_on_delete) {
    std::cout << "Found duplicate document id " << id << std::endl;
    s_s.RemoveDocument(id);
  }
}
