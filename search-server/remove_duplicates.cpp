#include "remove_duplicates.h"
void RemoveDuplicates(SearchServer &s_s){
  auto end_ = s_s.end();
  std::set<int> doc_on_delete;
  for(auto j = s_s.begin(); j != end_; ++j){
    auto id = *j;
    auto string_map = s_s.GetWordFrequencies(id);
    for(auto i = next(j); i != end_; ++i){
      auto curr_id = *i;
      auto curr_string_map = s_s.GetWordFrequencies(curr_id);
      size_t count_true = 0;

      if(string_map.size() != curr_string_map.size()) continue;

      for(auto [str, _]: string_map){
        if(curr_string_map.count(str)) ++count_true;
      }

      if(count_true == string_map.size()){


        doc_on_delete.insert(curr_id);
      }
    }
  }

  for(auto id : doc_on_delete) {
    std::cout << "Found duplicate document id " << id << std::endl;
    s_s.RemoveDocument(id);
  }
}
