#pragma once
#include "document.h"
#include <string>
#include <vector>
#include <tuple>
#include <map>
#include <set>
#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <numeric>
#include "string_processing.h"
#include <execution>
#include <string_view>
#include <functional>
#include "concurrent_map.h"
#include <thread>
#include <future>
#include <type_traits>

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double DEAD_ZONE = 1e-6;

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
     using Iterator_map  = typename std::map<int, std::map<std::string, double>>::iterator;
     using Iterator_id  = typename std::set<int>::iterator;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
    {
        if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
            throw std::invalid_argument("Some of stop words are invalid");
        }
    }

    explicit SearchServer(const std::string_view stop_words_text);
    explicit SearchServer(const std::string& stop_words_text);

    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);
    //Удаление документа
    void RemoveDocument(int document_id);
    //Удаление документа с execution

    void RemoveDocument(std::execution::sequenced_policy, int document_id);

    void RemoveDocument(std::execution::parallel_policy, int document_id);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, const std::string_view raw_query, DocumentPredicate document_predicate) const {
        std::string query_words = std::string(raw_query);

        const auto query = ParseQuery(query_words);
        auto matched_documents = FindAllDocuments(std::execution::seq, query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < DEAD_ZONE) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
        });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const {
        //std::string query_words = std::string(raw_query);
        //const auto query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(std::execution::par, ParseQuery(raw_query), document_predicate);

        sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < DEAD_ZONE) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
        });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }

    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, const std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, const std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, const std::string_view raw_query) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;


    int GetDocumentCount() const;

    int GetDocumentId(int index) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy, const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy,const std::string_view raw_query, int document_id) const;

    Iterator_id begin();
    Iterator_id end();

    const std::map<std::string_view, double> GetWordFrequencies(int document_id) const;

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
        std::string data;
    };

    const std::set<std::string, std::less<>> stop_words_;

    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    //Для быстрого возврата слов в документе по айди
    std::map<int, std::map<std::string_view, double>> id_words_freg_;
    //Для пустого возврата слов
    std::map<std::string_view, double> zero_res_;

    std::set<int> set_id_;

    std::map<int, DocumentData> documents_;


    //std::vector<int> document_ids_;

    bool IsStopWord(const std::string_view word) const;

    static bool IsValidWord(const std::string_view word);


    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);


    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(const std::string_view text) const;

    struct Query {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };

    struct VecQuery {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    std::string query_words_;

    Query ParseQuery(const std::string_view text) const;
    VecQuery ParseQueryVec(const std::string_view text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::sequenced_policy, const Query& query, DocumentPredicate document_predicate) const {
        std::map<int, double> document_to_relevance;
        for (const std::string_view word : query.plus_words) {
            //std::cout << "Word" <<word << std::endl;
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const auto word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
        }

        return matched_documents;
    }




      template <typename DocumentPredicate>
      std::vector<Document> FindAllDocuments(std::execution::parallel_policy , const Query& query, DocumentPredicate document_predicate) const {
        ConcurrentMap<int, double> document_to_relevance(101);
        ForEach(std::execution::par, query.plus_words, [this, &document_to_relevance, &document_predicate](const auto &word) {
            if (!(word_to_document_freqs_.count(word) == 0)) {
              const double inverse_document_freq = std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
              for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                  const auto& document_data = documents_.at(document_id);
                  if (document_predicate(document_id, document_data.status, document_data.rating)) {
                      document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                  }
              }
            }
        });


        ForEach(std::execution::par, query.minus_words, [this, &document_to_relevance](const auto &word){
            if (!(word_to_document_freqs_.count(word) == 0)) {
              for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
              }
            }
        });

        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
            matched_documents.push_back({std::move(document_id), std::move(relevance), documents_.at(document_id).rating});
        }

        return matched_documents;
        }
};




