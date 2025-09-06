/*
 task1.c
 Лабораторная работа 2, Задание 1, Вариант 5
 Описание: Ввести 2 символьные переменные a и b. Через указатель изменить значение a
 (например, сдвинуть символ на +1). Затем поменять местами a и b через указатели.
*/
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    char a, b;
    printf("enter two characters (separated by space or newline):\n");
    if (scanf(" %c %c", &a, &b) != 2) {
        fprintf(stderr, "Invalid input\n");
        return 1;
    }

    char *pa = &a, *pb = &b;

    printf("Before: a = '%c' (0x%02x), b = '%c' (0x%02x)\n", a, (unsigned char)a, b, (unsigned char)b);

    *pa = (char)((unsigned char)(*pa) + 1);

    printf("After change via pointer: a = '%c', b = '%c'\n", a, b);

    if (pa != pb) {
        *pa ^= *pb;
        *pb ^= *pa;
        *pa ^= *pb;
    }

    printf("After swap via pointers: a = '%c', b = '%c'\n", a, b);

    return 0;
}
