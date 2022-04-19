#include <algorithm>
#include <execution>
#include <vector>
#include <string>
#include <list>

#include "search_server.h"
#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {

    std::vector<std::vector<Document>> result_of_queries(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), result_of_queries.begin(),
        [&search_server](const std::string& query) { return search_server.FindTopDocuments(query); });
    return result_of_queries;
}

std::list<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {

    std::list<Document> list_documents_result;
    for (const auto& documents : ProcessQueries(search_server, queries))
    {
        std::list<Document> list_documents(make_move_iterator(documents.begin()), make_move_iterator(documents.end()));
        list_documents_result.splice(list_documents_result.end(), std::move(list_documents));
    }

    return list_documents_result;
}
