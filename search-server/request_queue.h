#pragma once
#include "search_server.h"
#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer &search_server);


    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        QueryResult result;
        result.doc_result = server_for_empty_stat.FindTopDocuments(raw_query, document_predicate);
        result.IsEmpty = IsFindDocEmpty(result.doc_result);
        PushPop(result);
        return result.doc_result;
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;
private:
    struct QueryResult {
        std::vector<Document> doc_result;
        bool IsEmpty;
    };

    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    int empty_query_ = 0;
    //Ссылка на сукмук
    const SearchServer &server_for_empty_stat;

    bool IsFindDocEmpty(const std::vector<Document> &find_result);

    void EmptyPlus(bool IsEmpty);

    void EmptyMinus(bool IsEmpty);

    void PushPop(const QueryResult &result);
};

