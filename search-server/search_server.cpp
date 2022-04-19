#include "search_server.h"
#include "string_processing.h"
#include <string>
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <string_view>
#include <execution>
#include <type_traits>

SearchServer::SearchServer(std::string_view stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))
{
}

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(std::string_view(stop_words_text))
{
}

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("Invalid document_id"s);
    }
    const auto [it, inserted_word] = documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status, std::string(document) });
    const auto words = SplitIntoWordsNoStop(it->second.text);
    const double inv_word_count = 1.0 / words.size();
    for (const std::string_view word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        frequencies_[document_id][word] += inv_word_count;
    }
    document_ids_.insert(document_id);

}

bool SearchServer::IsStopWord(std::string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(std::string_view word) {
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
    std::vector<std::string_view> words;
    for (std::string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}
int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty"s);
    }
    std::string_view word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("Query word is invalid");
    }
    return { word, is_minus, IsStopWord(word) };
}

std::vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy, std::string_view raw_query, DocumentStatus status) const {
     return FindTopDocuments(std::execution::seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating)
            {return document_status == status;
            });    
}

std::vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::par, raw_query, [status](int document_id, DocumentStatus document_status, int rating)
        {return document_status == status;
        });
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {

    return FindTopDocuments(std::execution::seq, raw_query, status);
}

std::vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy, std::string_view raw_query) const {
        return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy, std::string_view raw_query) const {
    return FindTopDocuments(std::execution::par, raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
    return FindTopDocuments(std::execution::seq, raw_query);
}

size_t SearchServer::GetDocumentCount() const {
    return documents_.size();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const std::map<std::string_view, double> map = {};
    if (documents_.count(document_id) == 0) {
        return map;
    }
    else {
        return frequencies_.at(document_id);
    }
}


const std::set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.cbegin();
}

const std::set<int>::const_iterator SearchServer::end() const {
    return document_ids_.cend();
}

void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(std::execution::seq, document_id);
}

void SearchServer::RemoveDocument(std::execution::sequenced_policy, int document_id) {
    if (documents_.count(document_id) == 0)
    {
        return;
    }
    document_ids_.erase(document_id);
    documents_.erase(document_id);
    for (auto& doc_freq : frequencies_.at(document_id))
    {
        word_to_document_freqs_[doc_freq.first].erase(document_id);
    }

    frequencies_.erase(document_id);
}

void SearchServer::RemoveDocument(std::execution::parallel_policy, int document_id) {
    if (documents_.count(document_id) == 0)
    {
        return;
    }
    document_ids_.erase(document_id);

    documents_.erase(document_id);

    auto word_freq = std::move(frequencies_.at(document_id));
    std::vector<std::string_view> words(word_freq.size());
    std::transform(std::execution::par,
        word_freq.begin(), word_freq.end(), words.begin(),
        [](const auto w_f) {
            return w_f.first;
        });

    std::for_each(std::execution::par, words.begin(), words.end(),
        [&document_id, this](const auto word) {
            word_to_document_freqs_.at(word).erase(document_id);
        });

    frequencies_.erase(document_id);
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text, bool need_sort) const
{
    Query query;

    auto  vector_words = SplitIntoWords(text);

    if (need_sort)
    {
        std::sort(std::execution::par, vector_words.begin(), vector_words.end());
        auto last = std::unique(vector_words.begin(), vector_words.end());
        vector_words.resize(std::distance(vector_words.begin(), last));
    }
    for (std::string_view word : vector_words) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            }
            else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }
    return query;
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy,
    std::string_view raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query, true);

    const auto status_doc = documents_.at(document_id).status;

    for (std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        auto& map = word_to_document_freqs_.at(word);
        if (map.count(document_id)) {
            return { {}, status_doc };
        }
    }
    std::vector<std::string_view> matched_words;
    for (std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        auto& map = word_to_document_freqs_.at(word);
        if (map.count(document_id)) {
            matched_words.push_back(word);
        }
    }

    return { matched_words, documents_.at(document_id).status };
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy,
    std::string_view raw_query, int document_id) const {

    const auto query = ParseQuery(raw_query, false);
    if (
        any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(),
            [this, &document_id](std::string_view word) {
                return (word_to_document_freqs_.count(word) == 0) ? false
                    : (frequencies_.at(document_id).count(word));
            })
        ) {
        return { std::vector<std::string_view>(), documents_.at(document_id).status };
    }

    std::vector<std::string_view> matched_words(query.plus_words.size());
    const auto last_copied_elem = copy_if(std::execution::par, move(query.plus_words.begin()), move(query.plus_words.end()), move(matched_words.begin()),
        [this, &document_id](std::string_view word) {
            return (word_to_document_freqs_.count(word) == 0) ? false
                : (word_to_document_freqs_.at(word).count(document_id));
        }
    );

    matched_words.resize(distance(matched_words.begin(), last_copied_elem));
    std::sort(std::execution::par, matched_words.begin(), matched_words.end());
    auto last = std::unique(matched_words.begin(), matched_words.end());
    matched_words.resize(std::distance(matched_words.begin(), last));

    return { matched_words, documents_.at(document_id).status };
}


std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::string_view raw_query,
    int document_id) const {
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
    return std::log(SearchServer::GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
