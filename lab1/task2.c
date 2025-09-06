#include <stdio.h>

int main(void) {
    int a, b, c, d;
    printf("Введите 4 целых числа (a b c d): ");
    if (scanf("%d %d %d %d", &a, &b, &c, &d) != 4) {
        printf("Неверный ввод\n");
        return 1;
    }

    int count = 0;
    count += (a > 0 && b > 0 && c > 0) ? 1 : 0;
    count += (a > 0 && b > 0 && d > 0) ? 1 : 0;
    count += (a > 0 && c > 0 && d > 0) ? 1 : 0;
    count += (b > 0 && c > 0 && d > 0) ? 1 : 0;

    printf("Количество троек положительных чисел: %d\n", count);
    return 0;
}
