#include "request_queue.h"

RequestQueue::RequestQueue(SearchServer &search_server) :server_for_empty_stat(search_server){}

std::vector<Document> RequestQueue::AddFindRequest(const std::string_view raw_query, DocumentStatus status) {
    QueryResult result;
    result.doc_result = server_for_empty_stat.FindTopDocuments(std::execution::seq, raw_query, status);
    result.IsEmpty = IsFindDocEmpty(result.doc_result);
    PushPop(result);
    return result.doc_result;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string_view raw_query) {
    QueryResult result;
    result.doc_result = server_for_empty_stat.FindTopDocuments(raw_query);
    result.IsEmpty = IsFindDocEmpty(result.doc_result);
    PushPop(result);
    return result.doc_result;
}

int RequestQueue::GetNoResultRequests() const {
    return empty_query_;
}

bool RequestQueue::IsFindDocEmpty(const std::vector<Document> &find_result) {
  if(find_result.size() > 0) return false;
  else return true;
}

void RequestQueue::EmptyPlus(bool IsEmpty){
  if(IsEmpty) ++empty_query_;
}

void RequestQueue::EmptyMinus(bool IsEmpty){
  if(empty_query_ > 0 ) --empty_query_;
}

void RequestQueue::PushPop(const QueryResult &result){
    if(requests_.size() < min_in_day_){
      requests_.push_back(result);
      EmptyPlus(result.IsEmpty);
    } else {
      QueryResult first_query = requests_.front();
      EmptyMinus(first_query.IsEmpty);
      requests_.pop_front();
      requests_.push_back(result);
      EmptyPlus(result.IsEmpty);
    }
}
