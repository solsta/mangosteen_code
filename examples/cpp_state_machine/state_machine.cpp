//
// Created by se00598 on 13/03/24.
//

#include <iostream>
#include <jemalloc/jemalloc.h>
#include "../../mangosteen_instrumentation.h"
#include "../../flat_combining/dr_annotations.h"

class Book{
public:
    std::string author;
    std::string name;
    int number_of_pages;

    void print_book_info(){
        std::cout << this->author << " " << " " << this->name << " " << this->number_of_pages << std::endl;
    }
};

int main() {

    // TODO  verify that new object is created correctly in both cases, using standard constructor and placement new.
    //void *ptr = malloc(sizeof(Book));
    //Book *book = new (ptr) Book();
    Book *book = new Book();

    printf("book addr: %p\n", book);

    //Mangosteen initialization
    mangosteen_args mangosteenArgs;
    mangosteenArgs.mode = SINGLE_THREAD;
    initialise_mangosteen(&mangosteenArgs);

    book->print_book_info();
    instrument_start();
    book->author = "Author";
    book->name = "Book Name";
    book->number_of_pages = 100;
    instrument_stop();

    book->print_book_info();

    return 0;
}