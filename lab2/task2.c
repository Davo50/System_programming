/*
 task2.c
 Лабораторная работа 2, Задание 2, Вариант 5
 Описание: Описать 2 указателя на целый тип. Выделить для них динамическую память.
 Присвоить произвольные значения в выделенные ячейки в операторе присвоения.
*/
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int *p = malloc(sizeof(int));
    int *q = malloc(sizeof(int));
    if (!p || !q) {
        fprintf(stderr, "Memory allocation failed\n");
        free(p); free(q);
        return 1;
    }

    *p = 42;
    *q = -7;

    printf("two dynamically allocated ints:\n");
    printf("*p = %d, *q = %d\n", *p, *q);

    free(p);
    free(q);
    return 0;
}
