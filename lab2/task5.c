/*
 task5.c
 Лабораторная работа 2, Задание 5, Вариант 5
 Описание: Создать функцию, которая возвращает среднее арифметическое трех чисел.
 Значение возвращается через указатель (результат в выходном аргументе).
 В главной программе вызвать функцию два раза с разными входными данными.
*/
#include <stdio.h>

void mean3(double a, double b, double c, double *out) {
    if (out) *out = (a + b + c) / 3.0;
}

int main(void) {
    double a, b, c;
    double res;

    printf("enter three numbers (first call): ");
    if (scanf("%lf %lf %lf", &a, &b, &c) != 3) {
        fprintf(stderr, "Invalid input\n");
        return 1;
    }
    mean3(a,b,c,&res);
    printf("Mean = %g\n", res);

    printf("Enter three numbers (second call): ");
    if (scanf("%lf %lf %lf", &a, &b, &c) != 3) {
        fprintf(stderr, "Invalid input\n");
        return 1;
    }
    mean3(a,b,c,&res);
    printf("Mean = %g\n", res);

    return 0;
}
