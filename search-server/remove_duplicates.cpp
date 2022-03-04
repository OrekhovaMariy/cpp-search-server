#include "search_server.h"
#include "remove_duplicates.h"
#include <iostream>
#include <map>
#include <vector>
#include <set>


void RemoveDuplicates(SearchServer& search_server) {
    std::vector<int> for_delete; 

    std::set<std::set<std::string>> str_from_all_doc; 

    for (const int document_id : search_server) {

        std::set<std::string> str_from_one_doc;

        const auto& words_from_doc_with_freq = search_server.GetWordFrequencies(document_id);
        for (const auto& word : words_from_doc_with_freq) {
            str_from_one_doc.insert(word.first);
        }
        if (str_from_all_doc.count(str_from_one_doc) == 0) {
            str_from_all_doc.insert(str_from_one_doc);
        }
        else {
            for_delete.push_back(document_id);
        }
    }

    for (int i : for_delete) {
        std::cout << "Found duplicate document id " << i << std::endl;
        search_server.RemoveDocument(i);
    }

}