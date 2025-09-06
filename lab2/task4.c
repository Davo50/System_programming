/*
 task4.c
 Лабораторная работа 2, Задание 4, Вариант 5
 Описание: Дан массив b(n). Переписать в массив C(n) положительные элементы b,
 деленные на 5 (со сжатием). Затем упорядочить методом выбора (selection sort) по возрастанию.
*/
#include <stdio.h>
#include <stdlib.h>

void selection_sort(double *arr, int m) {
    for (int i = 0; i < m - 1; ++i) {
        int min_idx = i;
        for (int j = i + 1; j < m; ++j)
            if (arr[j] < arr[min_idx]) min_idx = j;
        if (min_idx != i) {
            double tmp = arr[i];
            arr[i] = arr[min_idx];
            arr[min_idx] = tmp;
        }
    }
}

int main(void) {
    int n;
    printf("enter array length n: ");
    if (scanf("%d", &n) != 1 || n <= 0) {
        fprintf(stderr, "Invalid n\n");
        return 1;
    }

    double *b = malloc(n * sizeof(double));
    if (!b) { fprintf(stderr, "Memory allocation failed\n"); return 1; }

    printf("Enter %d numbers (can be integers or floats):\n", n);
    for (int i = 0; i < n; ++i) {
        if (scanf("%lf", &b[i]) != 1) {
            fprintf(stderr, "Invalid input\n");
            free(b);
            return 1;
        }
    }

    int m = 0;
    for (int i = 0; i < n; ++i) if (b[i] > 0.0) ++m;

    double *c = NULL;
    if (m > 0) {
        c = malloc(m * sizeof(double));
        if (!c) { fprintf(stderr, "Memory allocation failed\n"); free(b); return 1; }
        int k = 0;
        for (int i = 0; i < n; ++i) {
            if (b[i] > 0.0) {
                c[k++] = b[i] / 5.0;
            }
        }

        printf("Array C before sort (positive elements of b divided by 5):\n");
        for (int i = 0; i < m; ++i) printf("%g ", c[i]);
        printf("\n");

        selection_sort(c, m);

        printf("Array C after selection sort (ascending):\n");
        for (int i = 0; i < m; ++i) printf("%g ", c[i]);
        printf("\n");
    } else {
        printf("No positive elements in b -> C is empty\n");
    }

    free(b);
    free(c);
    return 0;
}
