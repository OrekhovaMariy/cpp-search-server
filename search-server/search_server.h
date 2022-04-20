#pragma once
#include <algorithm>
#include <execution>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <string_view>
#include <type_traits>
#include <future>
#include <iterator>

#include "string_processing.h"
#include "read_input_functions.h"
#include "document.h"
#include "concurrent_map.h"
#include "log_duration.h"

using namespace std::string_literals;
const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string& stop_words_text);
    explicit SearchServer(std::string_view stop_words_text);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate, typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query,
        DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query,
        DocumentPredicate document_predicate) const;
   
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;
    
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    size_t GetDocumentCount() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    const std::set<int>::const_iterator begin() const;
    const std::set<int>::const_iterator end() const;

    template <typename ExecutionPolicy>
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const ExecutionPolicy& policy,
        std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query,
        int document_id) const;

    template <typename ExecutionPolicy>
    void RemoveDocument(const ExecutionPolicy& policy, int document_id);
    void RemoveDocument(int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
        std::string text;
    };

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    const TransparentStringSet stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> frequencies_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

    bool IsStopWord(std::string_view word) const;
    static bool IsValidWord(std::string_view word);
    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;
    static int ComputeAverageRating(const std::vector<int>& ratings);

    QueryWord ParseQueryWord(std::string_view text) const;
    Query ParseQuery(std::string_view text, bool need_sort) const;

    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename DocumentPredicate, typename ExecutionPolicy>
    std::vector<Document> FindAllDocuments(const ExecutionPolicy& policy, const Query& query,
        DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query,
        DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
    if (!std::all_of(stop_words_.begin(), stop_words_.end(), SearchServer::IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query,
    DocumentPredicate document_predicate) const {
    if constexpr (std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>) {
        const auto query = SearchServer::ParseQuery(raw_query, true);
        auto matched_documents = SearchServer::FindAllDocuments(policy, query, document_predicate);
        std::sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                const double maxDifference = 1e-6;
                return lhs.relevance > rhs.relevance
                    || (std::abs(lhs.relevance - rhs.relevance) < maxDifference && lhs.rating > rhs.rating);
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }
    else {
        const auto query = SearchServer::ParseQuery(raw_query, true);
        auto matched_documents = SearchServer::FindAllDocuments(policy, query, document_predicate);
        std::sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                const double maxDifference = 1e-6;
                return lhs.relevance > rhs.relevance
                    || (std::abs(lhs.relevance - rhs.relevance) < maxDifference && lhs.rating > rhs.rating);
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
    DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindAllDocuments(const ExecutionPolicy& policy, const Query& query,
    DocumentPredicate document_predicate) const {
    if constexpr (std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>) {
        std::map<int, double> document_to_relevance;
        for (std::string_view word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = SearchServer::ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }
        for (std::string_view word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                { document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
    else {
        ConcurrentMap<int, double> document_to_relevance(document_ids_.size());
        std::for_each(std::execution::par,
            query.plus_words.begin(), query.plus_words.end(),
            [this, &document_to_relevance, &document_predicate](const std::string_view word) {
                if (word_to_document_freqs_.count(word) > 0) {
                    const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                    for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                        const auto& document_data = documents_.at(document_id);
                        if (document_predicate(document_id, document_data.status, document_data.rating)) {
                            document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;

                        }
                    }
                }
            });
        auto document_to_relevance_res = document_to_relevance.BuildOrdinaryMap();
        for (std::string_view word : query.minus_words) {
            if (word_to_document_freqs_.count(word) > 0) {
                for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                    document_to_relevance_res.erase(document_id);

                }
            }
        }
        std::vector<Document> matched_documents(document_to_relevance_res.size());
        for (const auto [document_id, relevance] : document_to_relevance_res) {
            matched_documents.emplace_back(std::move(document_id), std::move(relevance), std::move(documents_.at(document_id).rating));
        }
        return matched_documents;
    }
}

//template <typename DocumentPredicate>
//std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy, const Query& query,
//    DocumentPredicate document_predicate) const {
//
//    ConcurrentMap<int, double> document_to_relevance(document_ids_.size());
//
//    std::for_each(std::execution::par,
//        query.plus_words.begin(), query.plus_words.end(),
//        [this, &document_to_relevance, &document_predicate](const std::string_view word) {
//            
//            if (word_to_document_freqs_.count(word) > 0) {
//                
//                    const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
//                    for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
//                        const auto& document_data = documents_.at(document_id);
//                        if (document_predicate(document_id, document_data.status, document_data.rating)) {
//                            document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
//                        
//                    }
//                }
//            }
//        });
//
//    auto document_to_relevance_res = document_to_relevance.BuildOrdinaryMap();
//    for (std::string_view word : query.minus_words) {
//        
//        if (word_to_document_freqs_.count(word) > 0) {
//        
//            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
//                document_to_relevance_res.erase(document_id);
//
//            }
//        }
//    }
//    
//    std::vector<Document> matched_documents(document_to_relevance_res.size());
//    
//    for (const auto [document_id, relevance] : document_to_relevance_res) {
//        
//        matched_documents.emplace_back(std::move(document_id), std::move(relevance), std::move(documents_.at(document_id).rating));
//    }
//    return matched_documents;
//
//}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query,
    DocumentPredicate document_predicate) const {
    return FindAllDocuments(std::execution::seq, query, document_predicate);
}
