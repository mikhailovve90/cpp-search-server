#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server,
    const std::vector<std::string>& queries) {
      std::vector<std::vector<Document>> result(queries.size());

      std::transform(std::execution::par, std::begin(queries), std::end(queries), std::begin(result),
                   [&](const std::string curr_q) {return search_server.FindTopDocuments(std::execution::seq, curr_q); });

      return result;
}


std::deque<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries){
      auto vec_res = ProcessQueries(search_server, queries);
      std::deque<Document> result;
      for(auto curr_vec_res : vec_res) {
         std::for_each(std::begin(curr_vec_res), std::end(curr_vec_res),[&result](Document curr_doc){result.push_back(curr_doc);});
      }
      return result;
}
