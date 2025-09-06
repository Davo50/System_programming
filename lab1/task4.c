#include <stdio.h>

int main(void) {
    unsigned long long term = 1;
    unsigned long long sum = 0;
    int n = 1;
    term = 1;
    while (1) {
        if (n == 1) term = 1;
        else term *= (unsigned long long)n;

        if (term > 1000) break;

        sum += term;
        printf("%2d! = %llu, частичная сумма = %llu\n", n, term, sum);
        n++;
        if (n > 20) {
        }
    }

    printf("\nИтог: сумма факториалов до %d! (включительно) = %llu\n", n-1, sum);
    return 0;
}
