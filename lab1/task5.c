#include <stdio.h>
#include <string.h>
#include <ctype.h>

int main(void) {
    char s[1024];
    printf("Введите строку: ");
    if (!fgets(s, sizeof(s), stdin)) return 0;
    size_t len = strlen(s);
    if (len && s[len-1] == '\n') s[len-1] = '\0';

    int max_len = 0;
    int cur_len = 0;
    for (size_t i = 0; s[i] != '\0'; ++i) {
        if (!isspace((unsigned char)s[i])) {
            cur_len++;
        } else {
            if (cur_len > max_len) max_len = cur_len;
            cur_len = 0;
        }
    }
    if (cur_len > max_len) max_len = cur_len;

    printf("Длина самого длинного слова: %d\n", max_len);
    return 0;
}
