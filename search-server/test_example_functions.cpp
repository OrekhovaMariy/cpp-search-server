#include "test_example_functions.h"
#include "read_input_functions.h"
#include "search_server.h"

#include <utility>
#include <algorithm>

template <typename Data>
void Print(std::ostream& out, const Data& container) {
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
std::ostream& operator<<(std::ostream& out, const std::vector<Element>& container) {
    out << "[";
    Print(out, container);
    out << "]";
    return out;
}

template <typename SetElement>
std::ostream& operator<<(std::ostream& out, const std::set<SetElement>& container) {
    out << "{";
    Print(out, container);
    out << "}";
    return out;
}

template <typename MapS, typename MapF>
void Print(std::ostream& out, const std::map<MapF, MapS>& container) {
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
std::ostream& operator<<(std::ostream& out, const std::map<MapS, MapF>& container) {
    out << "{";
    Print(out, container);
    out << "}";
    return out;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
    const std::string& func, unsigned line, const std::string& hint) {
    if (t != u) {
        std::cerr << std::boolalpha;
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
    const std::string& hint) {
    if (!value) {
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void RunTestImpl(T t, const std::string& func) {
    /* Напишите недостающий код */
    t();
    std::cerr << func << " OK" << std::endl;
}


#define RUN_TEST(func) RunTestImpl((func), #func)


// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server("the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

// Разместите код остальных тестов здесь
// Докуметы, содержащие минус-слова, не должны включаться в результаты поиска
void TestExcludeMinusWordsFromAddedDocumentContent() {
    const int doc_id1 = 42;
    const std::string content1 = "cat in the city"s;
    const std::vector<int> ratings1 = { 1, 2, 3 };
    const int doc_id2 = 43;
    const std::string content2 = "moon in the spoon"s;
    const std::vector<int> ratings2 = { 2, 3, 4 };

    {
        SearchServer server("the"s);
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
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };


    {
        SearchServer server("and"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        auto [vec_doc, status] = server.MatchDocument("in the"s, 42);
        ASSERT_EQUAL(vec_doc[0], "in"s);
        ASSERT_EQUAL(vec_doc[1], "the"s);
    }


    {
        SearchServer server("and"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        auto [vec_doc, status] = server.MatchDocument("in the -cat"s, 42);
        ASSERT_EQUAL(vec_doc.size(), 0);
    }

    {
        SearchServer server("the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        auto [vec_doc, status] = server.MatchDocument(""s, 42);
        ASSERT_EQUAL(vec_doc.size(), 0);
    }
}


// Сортировка найденных документов по релевантности. Возвращаемые при поиске документов результаты должны быть отсортированы в порядке убывания релевантности.
void TestSortingDocumentContent() {
    const int doc_id1 = 42;
    const std::string content1 = "white cat and fashionable collar"s;
    const std::vector<int> ratings1 = { 1, 2, 3 };

    const int doc_id2 = 43;
    const std::string content2 = "fluffy cat fluffy tail"s;
    const std::vector<int> ratings2 = { 4, 5, 6 };

    const int doc_id3 = 44;
    const std::string content3 = "groomed dog expressive eyes"s;
    const std::vector<int> ratings3 = { 5, 6, 7 };

    {
        SearchServer server("and"s);
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
        SearchServer server("and"s);
        server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
        const auto found_docs = server.FindTopDocuments("in"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.rating, (1 + 2 + 3) / 3);
    }

    {
        SearchServer server("and"s);
        server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, { -1, -2, -3 });
        const auto found_docs = server.FindTopDocuments("in"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.rating, (-1 - 2 - 3) / 3);
    }

    {
        SearchServer server("and"s);
        server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, { 0 });
        const auto found_docs = server.FindTopDocuments("in"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.rating, 0);
    }
}

// Фильтрация результатов поиска с использованием предиката
void TestPredicateDocumentContent() {
    const int doc_id1 = 42;
    const std::string content1 = "cat in the city"s;
    const std::vector<int> ratings1 = { 1, 2, 3 };

    const int doc_id2 = 43;
    const std::string content2 = "moon in the spoon"s;
    const std::vector<int> ratings2 = { 4, 5, 6 };

    {
        SearchServer server("and"s);
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
        SearchServer server("the"s);
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
        SearchServer server("the"s);

        server.AddDocument(43, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
        server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
        const auto found_docs = server.FindTopDocuments("fluffy cat"s);
        const Document& doc0 = found_docs[0];
        const Document& doc1 = found_docs[1];
        ASSERT_EQUAL(doc0.id, 43);
        ASSERT_EQUAL(doc1.id, 42);
        ASSERT_HINT(abs(doc0.relevance - 0.3465735) < 1e-6, "Relevance must be count as TF-IDF"s);
        ASSERT_HINT(abs(doc1.relevance - 0) < 1e-6, "Relevance must be count as TF-IDF"s);
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
