#pragma once

#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <utility>
#include <algorithm>
#include <string>

#include "search_server.h"

void AddDocument(SearchServer& search_server, int document_id, std::string_view document,
    DocumentStatus status, const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, std::string_view raw_query);

void MatchDocuments(const SearchServer& search_server, std::string_view query); 

template <typename Data>
void Print(std::ostream& out, const Data& container);

template <typename Element>
std::ostream& operator<<(std::ostream& out, const std::vector<Element>& container);

template <typename SetElement>
std::ostream& operator<<(std::ostream& out, const std::set<SetElement>& container);

template <typename MapS, typename MapF>
void Print(std::ostream& out, const std::map<MapF, MapS>& container);

template <typename MapS, typename MapF>
std::ostream& operator<<(std::ostream& out, const std::map<MapS, MapF>& container);

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
    const std::string& func, unsigned line, const std::string& hint);

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
    const std::string& hint);

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void RunTestImpl(T t, const std::string& func);


#define RUN_TEST(func) RunTestImpl((func), #func)


// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent();
void TestExcludeMinusWordsFromAddedDocumentContent();


// Матчинг документов. При матчинге документа по поисковому запросу должны быть
// возвращены все слова из поискового запроса, присутствующие в документе.
// Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
void TestMatchingDocumentContent();

// Сортировка найденных документов по релевантности. Возвращаемые при поиске документов результаты должны быть отсортированы в порядке убывания релевантности.
void TestSortingDocumentContent();

// Рейтинг добавленного документа равен среднему арифметическому оценок документа
void TestAvarageRatingDocumentContent();

// Фильтрация результатов поиска с использованием предиката
void TestPredicateDocumentContent();
// Поиск документов, имеющих заданный статус
void TestRescueStatusDocumentContent();

// Корректное вычисление релевантности найденных документов
void TestRelevanceDocumentContent();


// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer();

