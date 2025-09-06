#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    srand((unsigned)time(NULL));

    int M[20];
    int evens = 0, odds = 0;
    for (int i = 0; i < 20; ++i) {
        M[i] = (rand() % 101) - 50;
        if (M[i] % 2 == 0) evens++;
        else odds++;
    }

    printf("Массив M[20]:\n");
    for (int i = 0; i < 20; ++i) {
        printf("%4d%s", M[i], (i%10==9) ? "\n" : " ");
    }
    printf("Чётных: %d, Нечётных: %d\n\n", evens, odds);

    int A[10][6];
    printf("Матрица A[10][6] (до и после замены отрицательных на abs):\n");
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 6; ++j) {
            A[i][j] = (rand() % 201) - 100;
        }
    }

    printf("До:\n");
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 6; ++j) printf("%5d", A[i][j]);
        printf("\n");
    }

    for (int i = 0; i < 10; ++i)
        for (int j = 0; j < 6; ++j)
            if (A[i][j] < 0) A[i][j] = -A[i][j];

    printf("\nПосле:\n");
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 6; ++j) printf("%5d", A[i][j]);
        printf("\n");
    }

    return 0;
}
