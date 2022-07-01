// search_server_s1_t2_v2.cpp
#
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cassert>
#include <numeric>
using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double DEAD_ZONE = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
    }

    vector<Document> FindTopDocuments(const string& raw_query, const DocumentStatus &status) const {
      auto find_doc = FindTopDocuments(raw_query, [status](int document_id, DocumentStatus dstatus, int rating) { return dstatus == status;});
      return find_doc;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        auto find_doc = FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
        return find_doc;
    }

    template <typename Condition>
    vector<Document> FindTopDocuments(const string& raw_query, Condition condition) const{
        const Query query = ParseQuery(raw_query);
        auto find_doc = FindAllDocuments(query);

        for(int i = 0; i < find_doc.size(); ++i){
            if(!condition(find_doc[i].id, documents_.at(find_doc[i].id).status, find_doc[i].rating)){
                find_doc.erase(find_doc.begin() + i);
                --i;
            }
        }

        sort(find_doc.begin(), find_doc.end(),
             [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < DEAD_ZONE) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
             });




        if (find_doc.size() > MAX_RESULT_DOCUMENT_COUNT) {
            find_doc.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return find_doc;
    }



    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        rating_sum = accumulate(ratings.begin(), ratings.end(), 0);

        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    vector<Document> FindAllDocuments(const Query& query, DocumentStatus status) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (documents_.at(document_id).status == status) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
            });
        }
        return matched_documents;
    }

    vector<Document> FindAllDocuments(const Query& query) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }

        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
            });
        }
        return matched_documents;
    }
};



// Всё Что касается тестирования
template <typename AnyVector>
ostream& operator<<(ostream &out, const vector<AnyVector> &container) {
    bool first_flag = false;
    out << "[";
    for (const auto element : container) {
        if(first_flag) out << ", "s << element ;
        else {
          out << element ;
          first_flag = true;
        }

    }
    out << "]";
    return out;
}

template <typename AnySet>
ostream& operator<<(ostream &out, const set<AnySet> &container) {
    bool first_flag = false;
    out << "{";
    for (const auto element : container) {
        if(first_flag) out << ", "s << element ;
        else {
          out << element ;
          first_flag = true;
        }

    }
    out << "}";
    return out;
}

template <typename AnyKey,typename AnyVal>
ostream& operator<<(ostream &out, const map<AnyKey, AnyVal> &container) {
    bool first_flag = false;
    out << "{";
    for (const auto [key, value] : container) {
        if(first_flag) out << ", "s << key <<": " << value ;
        else {
          out << key <<": " << value ;
          first_flag = true;
        }

    }
    out << "}";
    return out;
}



template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))



// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

void TestAddDoc() {
    const int doc_id0 = 42;
    const int doc_id1 = 43;
    const int doc_id2 = 44;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    SearchServer server;
    server.AddDocument(doc_id0, content, DocumentStatus::ACTUAL, ratings);

    ASSERT_HINT(server.GetDocumentCount() == 1, "Неправильно работает добавление файла.");
    auto found_docs = server.FindTopDocuments("cat"s);
    ASSERT_HINT(found_docs.size() == 1, "Неправильно работает добавление файла.");
    server.AddDocument(doc_id1, content, DocumentStatus::ACTUAL, ratings);
    found_docs = server.FindTopDocuments("cat"s);
    ASSERT_HINT(found_docs.size() == 2, "Неправильно работает добавление файла.");
    server.AddDocument(doc_id2, content, DocumentStatus::ACTUAL, ratings);
    found_docs = server.FindTopDocuments("cat"s);
    ASSERT_HINT(found_docs.size() == 3, "Неправильно работает добавление файла.");


    const Document& doc0 = found_docs[0];
    const Document& doc1 = found_docs[1];
    const Document& doc2 = found_docs[2];
    ASSERT_HINT(doc0.id == doc_id0, "Неверное добавление файла, айди добавленного не совпадает");
    ASSERT_HINT(doc1.id == doc_id1, "Неверное добавление файла, айди добавленного не совпадает");
    ASSERT_HINT(doc2.id == doc_id2, "Неверное добавление файла, айди добавленного не совпадает");


}

void TestMinusWord(){
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};


    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("city"s);
        ASSERT_HINT(found_docs.size() == 1, "Неправильно работает добавление файла.");
        const Document& doc0 = found_docs[0];
        ASSERT_HINT(doc0.id == doc_id, "Неверное добавление файла, айди добавленного не совпадает");
    }


    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("cat -city"s).empty(), "Не должны искаться документы содержащие минус слово" );
    }
}

void TestMatching() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};


       {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("city"s);
        ASSERT_HINT(found_docs.size() == 1, "Неправильно работает добавление файла.");
        const Document& doc0 = found_docs[0];
        ASSERT_HINT(doc0.id == doc_id, "Неверное добавление файла, айди добавленного не совпадает");
        const auto tuple_Match  = server.MatchDocument("cat in city", 42);
        vector<string> m_res = {"cat", "city" , "in" };
        auto [vec_matc, id_m] = tuple_Match;

        ASSERT_HINT(vec_matc == m_res, "Совпадение слов запроса и документо неверно");
        }


        {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("city"s);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id);
        const auto tuple_Match  = server.MatchDocument("cat in -city", 42);
        auto [vec_matc, id_m] = tuple_Match;
        vector<string> m_res = {};
        ASSERT_HINT(vec_matc == m_res, "Совпадение слов запроса и документо неверно при мину слове");
        }
}

void TestRelevance(){
    const int doc_id1 = 42;
    const int doc_id2 = 43;
    const int doc_id3 = 44;
    const string content1 = "cat in the city"s;
    const string content2 = "cat in the cat"s;
    const string content3 = "cat in the city and cat"s;

    const vector<int> ratings1 = {1, 2, 3};
    const vector<int> ratings2 = {1, 3, 3};
    const vector<int> ratings3 = {1, 5, 3};


       {
        SearchServer server;
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
        server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT(found_docs.size() == 3);
        const Document& doc1 = found_docs[0];
        const Document& doc2 = found_docs[1];
        const Document& doc3 = found_docs[2];
        ASSERT_HINT(doc1.id == doc_id3, "Неверная сортировка по релевантности");
        ASSERT_HINT(doc2.id == doc_id1, "Неверная сортировка по релевантности");
        ASSERT_HINT(doc3.id == doc_id2, "Неверная сортировка по релевантности");

        }
}

void TestAvRat(){
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings1 = {1, 2, 3, 6, 8, 7};
    const vector<int> ratings2 = {-1, -2, -3, -6, -8, -7};
    const vector<int> ratings3 = {-1, -2, -3, 6, 8, 7};


    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings1);
        const auto found_docs = server.FindTopDocuments("city"s);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        ASSERT_HINT(doc0.rating == 4, "Неверно исчисляется средний рейтинг");
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings2);
        const auto found_docs = server.FindTopDocuments("city"s);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        ASSERT_HINT(doc0.rating == -4, "Неверно исчисляется средний рейтинг c отрицательными");
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings3);
        const auto found_docs = server.FindTopDocuments("city"s);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        ASSERT_HINT(doc0.rating == 2, "Неверно исчисляется средний рейтинг cмешаный");
    }

}


void TestPredicate(){
    const int doc_id1 = 42;
    const int doc_id2 = 43;
    const int doc_id3 = 44;
    const string content1 = "cat in the city"s;
    const string content2 = "cat in the cat"s;
    const string content3 = "cat in the city and cat"s;

    const vector<int> ratings1 = {1, 2, 3};
    const vector<int> ratings2 = {1, 3, 3};
    const vector<int> ratings3 = {1, 5, 3};


    SearchServer server;
    server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
    server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
    server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);
    const auto found_docs = server.FindTopDocuments("city"s,[](int document_id, DocumentStatus , int ) {return document_id  == 42;});
    ASSERT(found_docs.size() == 1);
    const Document& doc1 = found_docs[0];
    ASSERT_HINT(doc1.id == 42, "Неверно исполняется предиктат");


}

void TestStat(){
    const int doc_id1 = 42;
    const int doc_id2 = 43;
    const int doc_id3 = 44;
    const int doc_id4 = 45;

    const string content1 = "cat in the city"s;
    const string content2 = "cat in the cat city"s;
    const string content3 = "cat in the city and cat"s;
    const string content4 = "cat in the city and dat"s;

    const vector<int> ratings1 = {1, 2, 3};
    const vector<int> ratings2 = {1, 3, 3};
    const vector<int> ratings3 = {1, 5, 3};
    const vector<int> ratings4 = {2, 5, 3};


    SearchServer server;
    server.AddDocument(doc_id1, content1, DocumentStatus::IRRELEVANT, ratings1);
    server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings2);
    server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);
    server.AddDocument(doc_id4, content4, DocumentStatus::REMOVED, ratings4);

    auto found_docs = server.FindTopDocuments("city"s , DocumentStatus::BANNED);
    ASSERT(found_docs.size() == 1);
    Document& doc1 = found_docs[0];
    ASSERT_HINT(doc1.id == 43, "Неверная сортировка по статусу");

    found_docs = server.FindTopDocuments("city"s , DocumentStatus::IRRELEVANT);
    ASSERT(found_docs.size() == 1);
    doc1 = found_docs[0];
    ASSERT_HINT(doc1.id == 42, "Неверная сортировка по статусу");

    found_docs = server.FindTopDocuments("city"s , DocumentStatus::ACTUAL);
    ASSERT(found_docs.size() == 1);
    doc1 = found_docs[0];
    ASSERT_HINT(doc1.id == 44, "Неверная сортировка по статусу");

    found_docs = server.FindTopDocuments("city"s , DocumentStatus::REMOVED);
    ASSERT(found_docs.size() == 1);
    doc1 = found_docs[0];
    ASSERT_HINT(doc1.id == 45, "Неверная сортировка по статусу");
}

void TestRelevanceCo(){
        double LOC_DEAD_ZONE = 1e-3;
        SearchServer server;
        server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
        server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});
        const auto res = server.FindTopDocuments("пушистый ухоженный кот"s);
        ASSERT_HINT(abs(res[0].relevance - 0.866) < LOC_DEAD_ZONE, "Неверно высчитывается релевантность");
}

template <typename T>
void RunTestImpl(T func, const string& f_name) {
    func();
    cerr << f_name << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)// напишите недостающий код

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDoc);
    RUN_TEST(TestMinusWord);
    RUN_TEST(TestMatching);
    RUN_TEST(TestRelevance);
    RUN_TEST(TestAvRat);
    RUN_TEST(TestPredicate);
    RUN_TEST(TestStat);
    RUN_TEST(TestRelevanceCo);
}

//-----------------------------------------Тесты кончились----------------------------------------


// Для отображения
void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

int main() {
    TestSearchServer();
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }

    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }

    return 0;
}

