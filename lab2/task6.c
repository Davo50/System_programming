/*
 task6.c
 Лабораторная работа 2, Задание 6, Вариант 5
 Описание: Описать структуру книги (author, title, publisher, year, price).
 Заполнить массив (10 записей — можно использовать тестовые данные), переписать
 в новый массив только книги, в названии которых содержится ровно 3 буквы 'о'
 (учтены латинские 'o'/'O' и кириллические 'о'/'О' в UTF-8), затем отсортировать
 новый массив по названию издательства по алфавиту и вывести.
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

static void trim_newline(char *s) {
    size_t l = strlen(s);
    if (l && s[l-1] == '\n') s[l-1] = '\0';
}

/* Подсчёт букв 'o'/'O' (латинские) и кириллической 'о'/'О' в UTF-8 (D0 BE / D0 9E) */
static int count_o_utf8(const char *s) {
    int cnt = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        if (*p == 'o' || *p == 'O') {
            ++cnt;
            ++p;
        } else if (*p == 0xD0) {
            /* возможный кириллический символ в диапазоне D0 ?? */
            if (p[1]) {
                if (p[1] == 0xBE || p[1] == 0x9E) { /* 'о' U+043E -> D0 BE, 'О' U+041E -> D0 9E */
                    ++cnt;
                }
                p += 2;
            } else {
                ++p;
            }
        } else {
            ++p;
        }
    }
    return cnt;
}

static int cmp_publisher(const void *a, const void *b) {
    const Book *x = a;
    const Book *y = b;
    return strcmp(x->publisher, y->publisher);
}

int main(void) {
    const int N = 10;
    Book *arr = malloc(N * sizeof(Book));
    if (!arr) { fprintf(stderr, "Memory allocation failed\n"); return 1; }

    printf("up to %d books.\n", N);
    printf("If you want to use built-in sample data, press Enter on the first 'author' prompt.\n");

    int filled = 0;
    char buf[256];

    for (int i = 0; i < N; ++i) {
        printf("Enter author for book %d (or blank to finish / blank on first -> use sample): ", i+1);
        if (!fgets(arr[i].author, sizeof(arr[i].author), stdin)) break;
        trim_newline(arr[i].author);

        if (strlen(arr[i].author) == 0) {
            if (i == 0) {
                /* use sample data */
                Book sample[] = {
                    {"Author A", "О, одно слово", "AlphaPub", 2001, 10.50},
                    {"Author B", "Second book", "ZetaPub", 1999, 12.00},
                    {"Author C", "Mooro", "BetaPub", 2010, 8.99},
                    {"Author D", "Oooops", "GammaPub", 2005, 15.00},
                    {"Author E", "Book of ooo", "AlphaPub", 2018, 20.00},
                    {"Author F", "No o here", "DeltaPub", 2020, 5.00}
                };
                int s = (int)(sizeof(sample)/sizeof(sample[0]));
                for (int k = 0; k < s && k < N; ++k) arr[k] = sample[k];
                filled = s;
            }
            break;
        }

        /* title */
        printf("Enter title: ");
        if (!fgets(arr[i].title, sizeof(arr[i].title), stdin)) { filled = i; break; }
        trim_newline(arr[i].title);

        /* publisher */
        printf("Enter publisher: ");
        if (!fgets(arr[i].publisher, sizeof(arr[i].publisher), stdin)) { filled = i; break; }
        trim_newline(arr[i].publisher);

        /* year and price using fgets + sscanf to avoid leftover newline issues */
        printf("Enter year and price (e.g. 1999 12.50): ");
        if (!fgets(buf, sizeof(buf), stdin)) { fprintf(stderr, "Invalid input\n"); free(arr); return 1; }
        if (sscanf(buf, "%d %lf", &arr[i].year, &arr[i].price) != 2) {
            fprintf(stderr, "Invalid year/price format\n");
            free(arr);
            return 1;
        }

        filled = i + 1;
    }

    if (filled == 0) {
        /* If user pressed blank on first prompt, sample was loaded above and filled set.
           But in case nothing loaded, also fallback to sample: */
        Book sample[] = {
            {"Author A", "О, одно слово", "AlphaPub", 2001, 10.50},
            {"Author B", "Second book", "ZetaPub", 1999, 12.00},
            {"Author C", "Mooro", "BetaPub", 2010, 8.99},
            {"Author D", "Oooops", "GammaPub", 2005, 15.00},
            {"Author E", "Book of ooo", "AlphaPub", 2018, 20.00},
            {"Author F", "No o here", "DeltaPub", 2020, 5.00}
        };
        int s = (int)(sizeof(sample)/sizeof(sample[0]));
        for (int k = 0; k < s && k < N; ++k) arr[k] = sample[k];
        filled = s;
    }

    /* select books with exactly 3 'o' letters (latin or cyrillic) */
    Book *sel = malloc(filled * sizeof(Book));
    if (!sel) { fprintf(stderr, "Memory allocation failed\n"); free(arr); return 1; }
    int m = 0;
    for (int i = 0; i < filled; ++i) {
        int cnt = count_o_utf8(arr[i].title);
        if (cnt == 3) {
            sel[m++] = arr[i];
        }
    }

    if (m == 0) {
        printf("No books found with exactly 3 letters 'o' in title.\n");
    } else {
        qsort(sel, m, sizeof(Book), cmp_publisher);
        printf("Selected books (sorted by publisher):\n");
        for (int i = 0; i < m; ++i) {
            printf("%d) Author: %s\n   Title: %s\n   Publisher: %s, Year: %d, Price: %.2f\n",
                   i+1, sel[i].author, sel[i].title, sel[i].publisher, sel[i].year, sel[i].price);
        }
    }

    free(sel);
    free(arr);
    return 0;
}
