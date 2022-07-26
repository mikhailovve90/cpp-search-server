// search_server_s3_t3_v3.cpp

#include <deque>
#include "document.h"
#include "paginator.h"
#include "read_input_functions.h"
#include "search_server.h"
#include "request_queue.h"
#include <iostream>
#include "log_duration.h"
#include "remove_duplicates.h"
using namespace std;


template <typename Container>
ostream& operator<<(ostream& output, const IteratorRange<Container> &page) {
    for(auto i = page.begin(); i != page.end() ; ++i){
      output << *i;
    }
    return output;
}

std::ostream& operator<<(std::ostream& output, const Document& document){
    output << "{ "
         << "document_id = " << document.id << ", "
         << "relevance = " << document.relevance << ", "
         << "rating = " << document.rating << " }";
    return output;
}


void FindTopDocuments(SearchServer &search_server, const string &requery) {
    LogDuration a = LogDuration("Результаты поиска по запросу: " + requery, std::cerr);

    auto find_docs = search_server.FindTopDocuments(requery);
    for(auto doc : find_docs) cout << doc << endl;
}



/*void MatchDocuments(SearchServer &search_server, const string &requery) {
    //LogDuration("Матчинг документов по запросу: " + requery, std::cerr);
    LOG_DURATION_STREAM("Матчинг документов по запросу: " + requery, std::cerr);
    int count = search_server.GetDocumentCount();
    for(int i = 0; i < count; ++i){
      auto [words, status] = search_server.MatchDocument(requery, search_server.GetDocumentId(i));
      std::cout << "{document_id =" << search_server.GetDocumentId(i);
      std::cout << ", status = "<< static_cast<int>(status);
      std::cout << ", word = ";
      for(auto word : words) cout << word << " ";
      std::cout << "}" << std::endl;
    }
}*/

void AddDocument(SearchServer &s_s, int id, std::string data, DocumentStatus stat, std::vector<int> rating){
  s_s.AddDocument(id, data, stat, rating);

}







int main() {
    SearchServer search_server("and with"s);

    AddDocument(search_server, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
    AddDocument(search_server, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // дубликат документа 2, будет удалён
    AddDocument(search_server, 3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // отличие только в стоп-словах, считаем дубликатом
    AddDocument(search_server, 4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // множество слов такое же, считаем дубликатом документа 1
    AddDocument(search_server, 5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, {1, 2});

    // добавились новые слова, дубликатом не является
    AddDocument(search_server, 6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, {1, 2});

    // множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
    AddDocument(search_server, 7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, {1, 2});

    // есть не все слова, не является дубликатом
    AddDocument(search_server, 8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, {1, 2});

    // слова из разных документов, не является дубликатом
    AddDocument(search_server, 9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    cout << "Before duplicates removed: "s << search_server.GetDocumentCount() << endl;
    RemoveDuplicates(search_server);
    cout << "After duplicates removed: "s << search_server.GetDocumentCount() << endl;
    //const std::map<std::string, double> zero_res = search_server.GetWordFrequencies(1);


}
