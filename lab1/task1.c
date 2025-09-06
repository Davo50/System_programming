#include <stdio.h>
#include <string.h>

int main(void) {
    char name[100];
    printf("Введите фамилию и имя: ");
    if (fgets(name, sizeof(name), stdin) == NULL) return 0;
    size_t ln = strlen(name);
    if (ln && name[ln-1] == '\n') name[ln-1] = '\0';

    char ch = 'E';
    int i1 = 727, i2 = -968;
    double d1 = 374.652, d2 = -776.23;
    int last = 108;

    printf("'%c'; 'Меня зовут %s'; %d, %d; %.3f, %.3f; %d\n",
           ch, name, i1, i2, d1, d2, last);

    return 0;
}
