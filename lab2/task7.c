/*
 task7.c
 Лабораторная работа 2, Задание 7, Вариант 5
 Описание: Работать через функции: инициализация массива структур книги, выборка тех,
 у которых в названии ровно 3 буквы 'o', сортировка по издательству и вывод.
 Значение, изменяющееся внутри функции, возвращается через указатель.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char author[64];
    char title[128];
    char publisher[64];
    int year;
    double price;
} Book;

void init_books(Book **arr_ptr, int *n_ptr) {
    int n = 6;
    Book *arr = malloc(n * sizeof(Book));
    if (!arr) { *arr_ptr = NULL; *n_ptr = 0; return; }
    strcpy(arr[0].author, "Author A"); strcpy(arr[0].title, "Ooo Tales"); strcpy(arr[0].publisher, "AlphaPub"); arr[0].year=2001; arr[0].price=10.5;
    strcpy(arr[1].author, "Author B"); strcpy(arr[1].title, "Second book"); strcpy(arr[1].publisher, "ZetaPub"); arr[1].year=1999; arr[1].price=12.0;
    strcpy(arr[2].author, "Author C"); strcpy(arr[2].title, "Mooro"); strcpy(arr[2].publisher, "BetaPub"); arr[2].year=2010; arr[2].price=8.99;
    strcpy(arr[3].author, "Author D"); strcpy(arr[3].title, "Oooops"); strcpy(arr[3].publisher, "GammaPub"); arr[3].year=2005; arr[3].price=15.0;
    strcpy(arr[4].author, "Author E"); strcpy(arr[4].title, "Book of ooo"); strcpy(arr[4].publisher, "AlphaPub"); arr[4].year=2018; arr[4].price=20.0;
    strcpy(arr[5].author, "Author F"); strcpy(arr[5].title, "No o here"); strcpy(arr[5].publisher, "DeltaPub"); arr[5].year=2020; arr[5].price=5.0;
    *arr_ptr = arr;
    *n_ptr = n;
}

void free_books(Book **arr_ptr) {
    if (arr_ptr && *arr_ptr) {
        free(*arr_ptr);
        *arr_ptr = NULL;
    }
}

int count_o(const char *s) {
    int cnt = 0;
    for (; *s; ++s) if (*s == 'o' || *s == 'O') ++cnt;
    return cnt;
}

void select_books(Book *src, int n, Book **dest_ptr, int *m_ptr) {
    Book *dest = malloc(n * sizeof(Book));
    if (!dest) { *dest_ptr = NULL; *m_ptr = 0; return; }
    int k = 0;
    for (int i = 0; i < n; ++i) {
        if (count_o(src[i].title) == 3) {
            dest[k++] = src[i];
        }
    }
    *dest_ptr = dest;
    *m_ptr = k;
}

int cmp_publisher(const void *a, const void *b) {
    const Book *x = a; const Book *y = b;
    return strcmp(x->publisher, y->publisher);
}

void sort_books(Book *arr, int n) {
    qsort(arr, n, sizeof(Book), cmp_publisher);
}

void print_books(Book *arr, int n) {
    if (n == 0) { printf("No books to display.\n"); return; }
    for (int i = 0; i < n; ++i) {
        printf("%d) Author: %s\n   Title: %s\n   Publisher: %s, Year: %d, Price: %.2f\n",
               i+1, arr[i].author, arr[i].title, arr[i].publisher, arr[i].year, arr[i].price);
    }
}

int main(void) {
    Book *library = NULL;
    int n = 0;
    init_books(&library, &n);
    if (!library || n == 0) { fprintf(stderr, "Failed to initialize books\n"); return 1; }

    Book *selected = NULL;
    int m = 0;
    select_books(library, n, &selected, &m);
    if (!selected && m == 0) {
        printf("No selection.\n");
        free_books(&library);
        return 0;
    }

    if (m > 1) sort_books(selected, m);

    printf("Selected books (count = %d), sorted by publisher:\n", m);
    print_books(selected, m);

    free(selected);
    free_books(&library);
    return 0;
}
