/*
 task6.c
 Лабораторная работа 2, модифицированное Задание (ваш вариант)
 Описание:
  Определить комбинированный (структурный) тип для представления анкеты жителя,
  состоящей из его фамилии, названия города, где он проживает, и городского адреса.
  Адрес состоит из полей: "улица", "дом", "квартира".
  Ввести информацию по до 100 жителям. Вывести фамилии жителей, которые живут
  в одном городе с первым жителем из списка.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_RESIDENTS 100
#define MAX_NAME 64
#define MAX_CITY 64
#define MAX_STREET 64
#define MAX_HOUSE 16
#define MAX_APT 16

typedef struct {
    char surname[MAX_NAME];
    char city[MAX_CITY];
    struct {
        char street[MAX_STREET];
        char house[MAX_HOUSE];
        char apt[MAX_APT];
    } addr;
} Resident;

void trim_newline(char *s) {
    if (!s) return;
    size_t l = strlen(s);
    while (l > 0 && (s[l-1] == '\n' || s[l-1] == '\r')) {
        s[--l] = '\0';
    }
}

void trim_spaces(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len-1])) s[--len] = '\0';
    size_t i = 0;
    while (s[i] && isspace((unsigned char)s[i])) i++;
    if (i > 0) memmove(s, s + i, strlen(s + i) + 1);
}

int ci_equal(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a;
        unsigned char cb = (unsigned char)*b;
        if (tolower(ca) != tolower(cb)) return 0;
        a++; b++;
    }
    return (*a == '\0' && *b == '\0');
}

int prompt_readline(const char *prompt, char *buf, size_t size) {
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }
    if (!fgets(buf, (int)size, stdin)) return 0;
    trim_newline(buf);
    return 1;
}

int main(void) {
    Resident *arr = malloc(MAX_RESIDENTS * sizeof(Resident));
    if (!arr) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    printf("Введите данные до %d жителей.\n", MAX_RESIDENTS);
    printf("На первом шаге: если нажмете Enter (пустую строку) — будут загружены примерные данные.\n\n");

    int filled = 0;
    char tmp[256];

    for (int i = 0; i < MAX_RESIDENTS; ++i) {
        char prompt[128];
        snprintf(prompt, sizeof(prompt), "Resident %d — фамилия (Enter чтобы закончить ввод): ", i + 1);
        if (!prompt_readline(prompt, arr[i].surname, sizeof(arr[i].surname))) {
            break;
        }
        trim_spaces(arr[i].surname);
        if (strlen(arr[i].surname) == 0) {
            if (i == 0) {
                Resident sample[] = {
                    { "Ivanov", "Moscow", { "Tverskaya", "12", "34" } },
                    { "Petrov", "Saint-Petersburg", { "Nevsky", "24", "12" } },
                    { "Sidorov", "Moscow", { "Arbat", "10", "5" } },
                    { "Kuznetsov", "Kazan", { "Baumana", "3", "1" } },
                    { "Smirnova", "Moscow", { "Lenina", "7", "9" } },
                    { "Volkov", "Sochi", { "Primorskaya", "100", "2" } }
                };
                int s = (int)(sizeof(sample) / sizeof(sample[0]));
                for (int k = 0; k < s && k < MAX_RESIDENTS; ++k) arr[k] = sample[k];
                filled = s;
                printf("Загружено %d примерных записей.\n\n", filled);
            }
            break;
        }

        if (!prompt_readline("  Город: ", arr[i].city, sizeof(arr[i].city))) { free(arr); return 1; }
        trim_spaces(arr[i].city);

        if (!prompt_readline("  Улица: ", arr[i].addr.street, sizeof(arr[i].addr.street))) { free(arr); return 1; }
        trim_spaces(arr[i].addr.street);

        if (!prompt_readline("  Дом: ", arr[i].addr.house, sizeof(arr[i].addr.house))) { free(arr); return 1; }
        trim_spaces(arr[i].addr.house);

        if (!prompt_readline("  Квартира: ", arr[i].addr.apt, sizeof(arr[i].addr.apt))) { free(arr); return 1; }
        trim_spaces(arr[i].addr.apt);

        filled = i + 1;
        printf("\n");
    }

    if (filled == 0) {
        Resident sample[] = {
            { "Ivanov", "Moscow", { "Tverskaya", "12", "34" } },
            { "Petrov", "Saint-Petersburg", { "Nevsky", "24", "12" } },
            { "Sidorov", "Moscow", { "Arbat", "10", "5" } },
            { "Kuznetsov", "Kazan", { "Baumana", "3", "1" } },
            { "Smirnova", "Moscow", { "Lenina", "7", "9" } },
            { "Volkov", "Sochi", { "Primorskaya", "100", "2" } }
        };
        int s = (int)(sizeof(sample) / sizeof(sample[0]));
        for (int k = 0; k < s && k < MAX_RESIDENTS; ++k) arr[k] = sample[k];
        filled = s;
        printf("Ни одной записи введено — загружено %d примерных записей.\n\n", filled);
    }

    const char *city0 = arr[0].city;
    if (!city0 || strlen(city0) == 0) {
        printf("У первого жителя не указано название города — нечего сравнивать.\n");
        free(arr);
        return 0;
    }

    char city0_norm[MAX_CITY];
    strncpy(city0_norm, city0, MAX_CITY - 1);
    city0_norm[MAX_CITY - 1] = '\0';
    trim_spaces(city0_norm);
    for (char *p = city0_norm; *p; ++p) *p = (char)tolower((unsigned char)*p);

    printf("Первый житель: фамилия='%s', город='%s'\n", arr[0].surname, arr[0].city);
    printf("Список фамилий жителей из того же города ('%s'):\n", arr[0].city);

    int found = 0;
    for (int i = 0; i < filled; ++i) {
        char cityi[MAX_CITY];
        strncpy(cityi, arr[i].city, MAX_CITY - 1); cityi[MAX_CITY - 1] = '\0';
        trim_spaces(cityi);
        for (char *p = cityi; *p; ++p) *p = (char)tolower((unsigned char)*p);

        if (strcmp(cityi, city0_norm) == 0) {
            printf("  %s\n", arr[i].surname);
            ++found;
        }
    }
    if (found == 0) {
        printf("  (нет жителей в том же городе)\n");
    } else {
        printf("Всего найдено: %d\n", found);
    }

    free(arr);
    return 0;
}
