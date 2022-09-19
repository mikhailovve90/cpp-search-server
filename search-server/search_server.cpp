#include "search_server.h"



SearchServer::SearchServer(const std::string_view stop_words_text)
  :SearchServer(SplitIntoWords(stop_words_text)){}

SearchServer::SearchServer(const std::string& stop_words_text)
  :SearchServer(SplitIntoWords(stop_words_text)){}

//Проверка слова на валидность и отсутствие недопустимых символов
bool SearchServer::IsValidWord(const std::string_view word) {
  return std::none_of(word.begin(), word.end(), [](char c) {return c >= '\0' && c < ' ';});
}

//Подсчёт среднего
int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
  if (ratings.empty()) {
    return 0;
  }

  int rating_sum = 0;
  //Накопленное значение всего вектора
  rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);

  return rating_sum / static_cast<int>(ratings.size());
}


//Добавление документа
void SearchServer::AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
  if ((document_id < 0) || (documents_.count(document_id) > 0)) {
      throw std::invalid_argument("Invalid document_id");
  }

  documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status, std::string(document)});

  const auto words = SplitIntoWordsNoStop(documents_[document_id].data);

  const double inv_word_count = 1.0 / words.size();
  for (const std::string_view word : words) {
      //std::cout << "Добавление документа " << word << std::endl;
      word_to_document_freqs_[word][document_id] += inv_word_count;
      // Мапа хранящие айди документов, слова и частоту их упоминания в запросе
      id_words_freg_[document_id][word] += inv_word_count;
  }

  //for(auto [key, val] : word_to_document_freqs_) std::cout << "Добавленые " << key << std::endl;
  //for(auto [key, val] : documents_) std::cout << "Данные документа " << val.data << std::endl;
  set_id_.insert(document_id);
}


//Удаление документа
void SearchServer::RemoveDocument(const int document_id){
  if(documents_.count(document_id)){
    for(const auto [key, _] : id_words_freg_[document_id]){
        auto remove_freqs_word = word_to_document_freqs_[key].find(document_id);
        word_to_document_freqs_[key].erase(remove_freqs_word);
    }
    //auto remove_iter_doc = documents_.find(document_id);
    documents_.erase(documents_.find(document_id));
    set_id_.erase(set_id_.find(document_id));
    id_words_freg_.erase(id_words_freg_.find(document_id));
  }
}

void SearchServer::RemoveDocument(std::execution::sequenced_policy, int document_id){
     RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(std::execution::parallel_policy, int document_id){
  if(documents_.count(document_id)){
    std::vector<std::string> keywords(id_words_freg_[document_id].size());
    size_t counter = 0;
    std::for_each(std::execution::par, id_words_freg_[document_id].begin(),
                  id_words_freg_[document_id].end(),
                  [&counter, &keywords](const auto& rhs) {
                    keywords[counter] = rhs.first;
                    ++counter;});

    std::for_each(std::execution::par,
                  keywords.begin(),
                  keywords.end(),
                  [&](const std::string& word) {
                  word_to_document_freqs_[word].erase(document_id);
    });

    documents_.erase(documents_.find(document_id));
    set_id_.erase(set_id_.find(document_id));
    id_words_freg_.erase(id_words_freg_.find(document_id));
  }
}

//Поиск документов
std::vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy, const std::string_view raw_query, DocumentStatus status) const {
  return FindTopDocuments(std::execution::seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
      return document_status == status;
  });
}

std::vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy,const std::string_view raw_query) const {
  return FindTopDocuments(std::execution::seq,raw_query, DocumentStatus::ACTUAL);
}


std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const {
  return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
      return document_status == status;
  });
}


std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query) const {
  return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}



std::vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy, const std::string_view raw_query, DocumentStatus status) const {
  return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
      return document_status == status;
  });
}
std::vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy,const std::string_view raw_query) const {
  return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}



int SearchServer::GetDocumentCount() const {
  return documents_.size();
}

SearchServer::Iterator_id SearchServer::begin(){ return  set_id_.begin();}
SearchServer::Iterator_id SearchServer::end(){ return  set_id_.end();}

const std::map<std::string_view, double> SearchServer::GetWordFrequencies(int document_id) const{
  std::map<std::string_view, double> result;
  if(id_words_freg_.count(document_id)){
     for(const auto [key, value] : id_words_freg_.at(document_id)){
       result[key] = value;}

    return result;
  }

  return zero_res_;

}


//Совпадающие слова в документах
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string_view raw_query, int document_id) const {
  std::string query_words = std::string(raw_query);

  const auto query = ParseQuery(query_words);
  std::vector<std::string_view> matched_words;

  for (const std::string_view word : query.minus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
          continue;
      }
      if (word_to_document_freqs_.at(word).count(document_id)) {
          matched_words.clear();
          return {matched_words, documents_.at(document_id).status};
      }
  }

  matched_words.reserve(query.plus_words.size());
  std::for_each(query.plus_words.begin(), query.plus_words.end(), [this, &document_id, &matched_words](const auto rhs_str){
  auto temp_doc = word_to_document_freqs_.find(rhs_str);
    if(temp_doc != word_to_document_freqs_.end()){
      if(temp_doc->second.count(document_id)){
       matched_words.push_back(temp_doc->first);
      }
    }
  });

  //matched_words.resize(index);
  return {matched_words, documents_.at(document_id).status};
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy, const std::string_view raw_query, int document_id) const {
  return MatchDocument(raw_query, document_id);}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy,const std::string_view raw_query, int document_id) const{
  if (!documents_.count(document_id)) {
  throw std::out_of_range("No valid id" + std::to_string(document_id));}

  const auto query = ParseQueryVec(raw_query);
  std::vector<std::string_view> matched_words;
    for (const auto word : query.minus_words){
       auto temp_doc = word_to_document_freqs_.find(word);
       if(temp_doc == word_to_document_freqs_.end()) {
          continue;
      }
      if (temp_doc->second.count(document_id)) {
          matched_words.clear();
          return {matched_words, documents_.at(document_id).status};
      }
  }

  matched_words.resize(query.plus_words.size());
  size_t index = 0;

  std::for_each(query.plus_words.begin(), query.plus_words.end(), [this, &document_id, &matched_words, &index](const auto rhs_str){
  auto temp_doc = word_to_document_freqs_.find(rhs_str);
    if(temp_doc != word_to_document_freqs_.end()){
      if(temp_doc->second.count(document_id)){
       matched_words[index] = temp_doc->first;
       ++index;}
      }
   });

  //Удаляю дубликаты
  std::set<std::string_view> s;
  for(unsigned i = 0; i < index; ++i) s.insert(matched_words[i]);
  matched_words.resize(s.size());
  index = 0;
  for(auto w : s) {matched_words[index] = w; ++index;}
  //copy(std::execution::par, s.begin(), s.end(), std::begin(matched_words));

  return {matched_words, documents_.at(document_id).status};
}



//Если есть в стоп словах
bool SearchServer::IsStopWord(const std::string_view word) const {
  return stop_words_.count(word) > 0;
}


//Переделка строки в вектор без стоп слов
std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string_view text) const {
  std::vector<std::string_view> words;
  for (const std::string_view word : SplitIntoWords(text)) {
      if (!IsValidWord(word)) {
          throw std::invalid_argument("Word " + std::string(word) + " is invalid");
      }
      if (!IsStopWord(word)) {
          words.push_back(word);
      }
  }
  return words;
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string_view text) const {
  if (text.empty()) {
      throw std::invalid_argument("Query word is empty");
  }

  std::string_view word = text;
  bool is_minus = false;
  if (word[0] == '-') {
      is_minus = true;
      word = word.substr(1);
  }
  if (word.empty() ||  word[0] == '-' || !IsValidWord(word)) {
      throw std::invalid_argument("Query word " + std::string(text) + " is invalid");
  }

  return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(const std::string_view text) const {
  Query result;

  for (const std::string_view word : SplitIntoWords(text)) {
      const auto query_word = ParseQueryWord(word);
      if (!query_word.is_stop) {
          if (query_word.is_minus) {
              result.minus_words.insert(query_word.data);
          } else {
              result.plus_words.insert(query_word.data);
          }
      }
  }
  return result;
}

SearchServer::VecQuery SearchServer::ParseQueryVec(const std::string_view text) const {
  VecQuery result;
  auto temp = SplitIntoWords(text);
  result.minus_words.reserve(temp.size());
  result.plus_words.reserve(temp.size());

  for (const auto word : temp) {
      const auto query_word = ParseQueryWord(word);
      if (!query_word.is_stop) {
          if (query_word.is_minus) {
              result.minus_words.push_back(query_word.data);
          } else {
              result.plus_words.push_back(query_word.data);
          }
      }
  }
  return result;

}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view word) const {
    return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

