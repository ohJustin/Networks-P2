//  header file needed? program could not reference functions.
#ifndef P1_HELPER_H
#define P1_HELPER_H

#include <vector>
#include <string>

struct Book {
    std::string title;
    std::string author;
    std::string genre;
    bool available;
    int rating;
};

// LOAD BOOKS FROM FILE
std::vector<Book> loadBooksFromFile(const std::string& filename);

#endif
