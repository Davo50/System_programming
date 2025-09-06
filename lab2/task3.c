/*
 task3.c
 Лабораторная работа 2, Задание 3, Вариант 5
 Описание: Динамический массив a(n). Найти номер последнего элемента,
 меньшего заданного числа beta, количество положительных элементов и сумму элементов > 3.
*/
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int n;
    printf("enter array length n: ");
    if (scanf("%d", &n) != 1 || n <= 0) {
        fprintf(stderr, "Invalid n\n");
        return 1;
    }

    int *a = malloc(n * sizeof(int));
    if (!a) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    printf("Enter %d integers (space-separated or newline):\n", n);
    for (int i = 0; i < n; ++i) {
        if (scanf("%d", &a[i]) != 1) {
            fprintf(stderr, "Invalid input\n");
            free(a);
            return 1;
        }
    }

    int beta;
    printf("Enter beta (integer): ");
    if (scanf("%d", &beta) != 1) {
        fprintf(stderr, "Invalid beta\n");
        free(a);
        return 1;
    }

    int last_idx = -1;
    int count_pos = 0;
    long long sum_gt3 = 0;
    for (int i = 0; i < n; ++i) {
        if (a[i] < beta) last_idx = i;
        if (a[i] > 0) ++count_pos;
        if (a[i] > 3) sum_gt3 += a[i];
    }

    if (last_idx >= 0)
        printf("Index (0-based) of last element < %d : %d\n", beta, last_idx);
    else
        printf("No element found less than %d\n", beta);

    printf("Count of positive elements: %d\n", count_pos);
    printf("Sum of elements > 3: %lld\n", sum_gt3);

    free(a);
    return 0;
}
