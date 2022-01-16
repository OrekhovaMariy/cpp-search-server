#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

using namespace std;

/* Подставьте вашу реализацию класса SearchServer сюда */
const int MAX_RESULT_DOCUMENT_COUNT = 5;

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
vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        // Ваша реализация данного метода
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

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        // Ваша реализация данного метода
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;

    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        // Ваша реализация данного метода
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) { return document_status == status; });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        // Ваша реализация данного метода
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        // Ваша реализация данного метода
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        // Ваша реализация данного метода
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
        return { matched_words, documents_.at(document_id).status };
    }

private:
    // Реализация приватных методов вашей поисковой системы
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
        for (const int rating : ratings) {
            rating_sum += rating;
        }
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
                }
                else {
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

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
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

// Подставьте сюда вашу реализацию макросов
  // ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
template <typename Data>
void Print(ostream& out, const Data& container) {
    bool i = 0;
    for (const auto& element : container) {
        if (i == 1) {
            out << ", ";
        }
        out << element;
        i = 1;
    }
}

template <typename Element>
ostream& operator<<(ostream& out, const vector<Element>& container) {
    out << "[";
    Print(out, container);
    out << "]";
    return out;
}

template <typename SetElement>
ostream& operator<<(ostream& out, const set<SetElement>& container) {
    out << "{";
    Print(out, container);
    out << "}";
    return out;
}

template <typename MapS, typename MapF>
void Print(ostream& out, const map<MapF, MapS>& container) {
    bool i = 0;
    for (const auto& element : container) {
        if (i == 1) {
            out << ", ";
        }
        out << element.first << ": " << element.second;
        i = 1;
    }
}

template <typename MapS, typename MapF>
ostream& operator<<(ostream& out, const map<MapS, MapF>& container) {
    out << "{";
    Print(out, container);
    out << "}";
    return out;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
    const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void RunTestImpl(T t, const string& func) {
    /* Напишите недостающий код */
    t();
    cerr << func << " OK" << endl;
}


#define RUN_TEST(func) RunTestImpl((func), #func)

void Test1() {
}


// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
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

// Разместите код остальных тестов здесь
// Докуметы, содержащие минус-слова, не должны включаться в результаты поиска
void TestExcludeMinusWordsFromAddedDocumentContent() {
    const int doc_id1 = 42;
    const string content1 = "cat in the city"s;
    const vector<int> ratings1 = { 1, 2, 3 };
    const int doc_id2 = 43;
    const string content2 = "moon in the spoon"s;
    const vector<int> ratings2 = { 2, 3, 4 };

    {
        SearchServer server;
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
        ASSERT_EQUAL(server.FindTopDocuments("in -cat"s).size(), 1);
    }
}


// Матчинг документов. При матчинге документа по поисковому запросу должны быть
// возвращены все слова из поискового запроса, присутствующие в документе.
// Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
void TestMatchingDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };


    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        auto [vec_doc, status] = server.MatchDocument("in the"s, 42);
        ASSERT_EQUAL(vec_doc[0], "in"s);
        ASSERT_EQUAL(vec_doc[1], "the"s);
    }


    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        auto [vec_doc, status] = server.MatchDocument("in the -cat"s, 42);
        ASSERT_EQUAL(vec_doc.size(), 0);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        auto [vec_doc, status] = server.MatchDocument(""s, 42);
        ASSERT_EQUAL(vec_doc.size(), 0);
    }
}


// Сортировка найденных документов по релевантности. Возвращаемые при поиске документов результаты должны быть отсортированы в порядке убывания релевантности.
void TestSortingDocumentContent() {
    const int doc_id1 = 42;
    const string content1 = "white cat and fashionable collar"s;
    const vector<int> ratings1 = { 1, 2, 3 };

    const int doc_id2 = 43;
    const string content2 = "fluffy cat fluffy tail"s;
    const vector<int> ratings2 = { 4, 5, 6 };

    const int doc_id3 = 44;
    const string content3 = "groomed dog expressive eyes"s;
    const vector<int> ratings3 = { 5, 6, 7 };

    {
        SearchServer server;
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
        server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);
        const auto found_docs = server.FindTopDocuments("fluffy groomed cat"s);
        const Document& doc0 = found_docs[0];
        const Document& doc1 = found_docs[1];
        const Document& doc2 = found_docs[2];
        ASSERT_HINT(doc0.relevance > doc1.relevance, "Relevance must be count as TF-IDF"s);
        ASSERT_HINT(doc1.relevance > doc2.relevance, "Relevance must be count as TF-IDF"s);
    }
}

// Рейтинг добавленного документа равен среднему арифметическому оценок документа
void TestAvarageRatingDocumentContent() {

    {
        SearchServer server;
        server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
        const auto found_docs = server.FindTopDocuments("in"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.rating, (1 + 2 + 3) / 3);
    }

    {
        SearchServer server;
        server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {});
        const auto found_docs = server.FindTopDocuments("in"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.rating, 0);
    }
}

// Фильтрация результатов поиска с использованием предиката
void TestPredicateDocumentContent() {
    const int doc_id1 = 42;
    const string content1 = "cat in the city"s;
    const vector<int> ratings1 = { 1, 2, 3 };

    const int doc_id2 = 43;
    const string content2 = "moon in the spoon"s;
    const vector<int> ratings2 = { 4, 5, 6 };

    {
        SearchServer server;
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
        server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings2);
        const auto found_docs = server.FindTopDocuments("in the"s, [](int document_id, DocumentStatus document_status, int rating) { return document_status == DocumentStatus::ACTUAL; });
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id1);
    }
}

// Поиск документов, имеющих заданный статус
void TestRescueStatusDocumentContent() {

    {
        SearchServer server;
        server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
        server.AddDocument(43, "cat in the city"s, DocumentStatus::IRRELEVANT, { 1, 2, 3 });
        server.AddDocument(44, "cat in the city"s, DocumentStatus::BANNED, { 1, 2, 3 });
        server.AddDocument(45, "cat in the city"s, DocumentStatus::REMOVED, { 1, 2, 3 });
        const auto found_docs = server.FindTopDocuments("in"s, DocumentStatus::ACTUAL);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 42);
        const auto found_docs1 = server.FindTopDocuments("in"s, DocumentStatus::IRRELEVANT);
        const Document& doc1 = found_docs1[0];
        ASSERT_EQUAL(doc1.id, 43);
        const auto found_docs2 = server.FindTopDocuments("in"s, DocumentStatus::BANNED);
        const Document& doc2 = found_docs2[0];
        ASSERT_EQUAL(doc2.id, 44);
        const auto found_docs3 = server.FindTopDocuments("in"s, DocumentStatus::REMOVED);
        const Document& doc3 = found_docs3[0];
        ASSERT_EQUAL(doc3.id, 45);
    }
}

// Корректное вычисление релевантности найденных документов
void TestRelevanceDocumentContent() {

    {
        SearchServer server;

        server.AddDocument(43, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
        server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
        const auto found_docs = server.FindTopDocuments("fluffy cat"s);
        const Document& doc0 = found_docs[0];
        const Document& doc1 = found_docs[1];
        ASSERT_EQUAL(doc0.id, 43);
        ASSERT_EQUAL(doc1.id, 42);
        ASSERT_HINT(abs(doc0.relevance - 0.3465735) < 1e-6, "Relevance must be count as TF-IDF"s);
        ASSERT_HINT(doc1.relevance == 0, "Relevance must be count as TF-IDF"s);
    }
}


// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    // Не забудьте вызывать остальные тесты здесь    


    RUN_TEST(TestExcludeMinusWordsFromAddedDocumentContent);
    RUN_TEST(TestMatchingDocumentContent);
    RUN_TEST(TestSortingDocumentContent);
    RUN_TEST(TestAvarageRatingDocumentContent);
    RUN_TEST(TestRescueStatusDocumentContent);
    RUN_TEST(TestRelevanceDocumentContent);
    RUN_TEST(TestPredicateDocumentContent);

}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
