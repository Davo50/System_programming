#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

static int cmp_letters_case_insensitive(const void *pa, const void *pb);
static int cmp_long_desc(const void *pa, const void *pb);

void print_help() {
    printf("mapping_tool.exe - утилита для работы с memory-mapped файлами (WinAPI)\n");
    printf("Запуск без аргументов — интерактивный режим.\n");
    printf("Команды (через аргументы):\n");
    printf("  sortletters <path>   - упорядочить только буквы (оставляя не-буквы на месте)\n");
    printf("  countcase <path>     - посчитать строчные и прописные буквы\n");
    printf("  removea <path>       - удалить все 'a' и 'A' и в конец дописать сколько удалено\n");
    printf("  sortnums <path>      - отсортировать (по убыванию) числа в файле\n");
    printf("  help\n");
}

char* map_file_rw(const char* path, HANDLE* outFile, HANDLE* outMap, size_t* outSize) {
    *outFile = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (*outFile == INVALID_HANDLE_VALUE) {
        printf("CreateFile failed: %lu\n", GetLastError());
        return NULL;
    }
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(*outFile, &sz)) {
        printf("GetFileSizeEx failed: %lu\n", GetLastError());
        CloseHandle(*outFile);
        return NULL;
    }
    if (sz.QuadPart > (1LL<<31)) {
        printf("Файл слишком большой для demo mapping\n");
        CloseHandle(*outFile);
        return NULL;
    }
    *outSize = (size_t)sz.QuadPart;
    *outMap = CreateFileMappingA(*outFile, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (*outMap == NULL) {
        printf("CreateFileMapping failed: %lu\n", GetLastError());
        CloseHandle(*outFile);
        return NULL;
    }
    char* view = (char*)MapViewOfFile(*outMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (view == NULL) {
        printf("MapViewOfFile failed: %lu\n", GetLastError());
        CloseHandle(*outMap);
        CloseHandle(*outFile);
        return NULL;
    }
    return view;
}

static int cmp_letters_case_insensitive(const void *pa, const void *pb) {
    unsigned char a = *(const unsigned char*)pa;
    unsigned char b = *(const unsigned char*)pb;
    unsigned char la = (unsigned char)tolower(a);
    unsigned char lb = (unsigned char)tolower(b);
    if (la < lb) return -1;
    if (la > lb) return 1;
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

int cmd_sortletters(const char* path) {
    HANDLE hf, hm; size_t sz;
    char* view = map_file_rw(path, &hf, &hm, &sz);
    if (!view) return 1;

    size_t cap = 256, count = 0;
    size_t *indices = malloc(cap * sizeof(size_t));
    unsigned char *letters = malloc(cap * sizeof(unsigned char));
    if (!indices || !letters) {
        printf("Нет памяти\n");
        if (letters) free(letters);
        if (indices) free(indices);
        UnmapViewOfFile(view); CloseHandle(hm); CloseHandle(hf);
        return 1;
    }

    for (size_t i = 0; i < sz; ++i) {
        unsigned char c = (unsigned char)view[i];
        if (isalpha(c)) {
            if (count == cap) {
                cap *= 2;
                indices = realloc(indices, cap * sizeof(size_t));
                letters = realloc(letters, cap * sizeof(unsigned char));
                if (!indices || !letters) {
                    printf("Нет памяти при расширении\n");
                    free(indices); free(letters);
                    UnmapViewOfFile(view); CloseHandle(hm); CloseHandle(hf);
                    return 1;
                }
            }
            indices[count] = i;
            letters[count] = c;
            ++count;
        }
    }

    if (count == 0) {
        printf("Букв не найдено в файле.\n");
        free(indices); free(letters);
        UnmapViewOfFile(view); CloseHandle(hm); CloseHandle(hf);
        return 0;
    }

    qsort(letters, count, sizeof(unsigned char), cmp_letters_case_insensitive);

    for (size_t k = 0; k < count; ++k) {
        view[indices[k]] = (char)letters[k];
    }

    UnmapViewOfFile(view);
    CloseHandle(hm);
    CloseHandle(hf);
    free(indices);
    free(letters);

    printf("Упорядочено %zu букв в файле %s (буквы отсортированы по алфавиту, регистр сохранён в символах).\n", count, path);
    return 0;
}

int cmd_countcase(const char* path) {
    HANDLE hf, hm; size_t sz;
    char* view = map_file_rw(path, &hf, &hm, &sz);
    if (!view) return 1;
    size_t lower = 0, upper = 0;
    for (size_t i = 0; i < sz; ++i) {
        unsigned char c = (unsigned char)view[i];
        if (isalpha(c)) {
            if (islower(c)) ++lower;
            else if (isupper(c)) ++upper;
        }
    }
    UnmapViewOfFile(view);
    CloseHandle(hm);
    CloseHandle(hf);
    printf("В файле %s: строчных = %zu, прописных = %zu\n", path, lower, upper);
    return 0;
}

int cmd_removea(const char* path) {
    HANDLE hf = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) {
        printf("CreateFile failed: %lu\n", GetLastError());
        return 1;
    }
    LARGE_INTEGER szli;
    if (!GetFileSizeEx(hf, &szli)) {
        printf("GetFileSizeEx failed: %lu\n", GetLastError());
        CloseHandle(hf);
        return 1;
    }
    if (szli.QuadPart > (1LL<<31)) {
        printf("Файл слишком большой для demo\n");
        CloseHandle(hf);
        return 1;
    }
    size_t sz = (size_t)szli.QuadPart;

    HANDLE hm = CreateFileMappingA(hf, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (!hm) { printf("CreateFileMapping failed: %lu\n", GetLastError()); CloseHandle(hf); return 1; }
    char* view = (char*)MapViewOfFile(hm, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!view) { printf("MapViewOfFile failed: %lu\n", GetLastError()); CloseHandle(hm); CloseHandle(hf); return 1; }

    char *tmp = malloc(sz + 128);
    if (!tmp) { printf("Нет памяти\n"); UnmapViewOfFile(view); CloseHandle(hm); CloseHandle(hf); return 1; }

    size_t o = 0, removed = 0;
    for (size_t i = 0; i < sz; ++i) {
        unsigned char c = (unsigned char)view[i];
        if (c == 'a' || c == 'A') { ++removed; continue; }
        tmp[o++] = (char)c;
    }

    char suf[64];
    int slen = snprintf(suf, sizeof(suf), "\r\nRemoved: %zu\r\n", removed);
    size_t newSize = o + (size_t)slen;

    UnmapViewOfFile(view);
    CloseHandle(hm);

    LARGE_INTEGER pos;
    pos.QuadPart = (LONGLONG)newSize;
    if (!SetFilePointerEx(hf, pos, NULL, FILE_BEGIN) || !SetEndOfFile(hf)) {
        printf("Не удалось установить новый размер файла: %lu\n", GetLastError());
        CloseHandle(hf);
        free(tmp);
        return 1;
    }

    HANDLE hm2 = CreateFileMappingA(hf, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (!hm2) { printf("CreateFileMapping failed: %lu\n", GetLastError()); CloseHandle(hf); free(tmp); return 1; }
    char* view2 = (char*)MapViewOfFile(hm2, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!view2) { printf("MapViewOfFile failed: %lu\n", GetLastError()); CloseHandle(hm2); CloseHandle(hf); free(tmp); return 1; }

    memcpy(view2, tmp, o);
    memcpy(view2 + o, suf, slen);

    UnmapViewOfFile(view2);
    CloseHandle(hm2);
    CloseHandle(hf);
    free(tmp);

    printf("Удалено %zu букв 'a'/'A' из %s; в конец добавлено сообщение.\n", removed, path);
    return 0;
}

static int cmp_long_desc(const void *pa, const void *pb) {
    long a = *(const long*)pa;
    long b = *(const long*)pb;
    if (a < b) return 1;
    if (a > b) return -1;
    return 0;
}

int cmd_sortnums(const char* path) {
    HANDLE hf, hm; size_t sz;
    char* view = map_file_rw(path, &hf, &hm, &sz);
    if (!view) return 1;

    char *buf = malloc(sz + 1);
    if (!buf) { printf("Нет памяти\n"); UnmapViewOfFile(view); CloseHandle(hm); CloseHandle(hf); return 1; }
    memcpy(buf, view, sz);
    buf[sz] = 0;

    long *nums = NULL; size_t cnt = 0, cap = 0;
    char *p = buf;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) ++p;
        if (!*p) break;
        char *end;
        long val = strtol(p, &end, 10);
        if (end == p) { ++p; continue; }
        if (cnt == cap) {
            cap = cap ? cap * 2 : 64;
            nums = realloc(nums, cap * sizeof(long));
            if (!nums) { printf("Нет памяти\n"); free(buf); UnmapViewOfFile(view); CloseHandle(hm); CloseHandle(hf); return 1; }
        }
        nums[cnt++] = val;
        p = end;
    }

    if (cnt == 0) {
        printf("Чисел не найдено в файле.\n");
        free(buf);
        UnmapViewOfFile(view);
        CloseHandle(hm);
        CloseHandle(hf);
        if (nums) free(nums);
        return 0;
    }

    qsort(nums, cnt, sizeof(long), cmp_long_desc);

    size_t outcap = cnt * 24 + 16;
    char *out = malloc(outcap);
    if (!out) { printf("Нет памяти\n"); free(buf); free(nums); UnmapViewOfFile(view); CloseHandle(hm); CloseHandle(hf); return 1; }
    size_t pos = 0;
    for (size_t i = 0; i < cnt; ++i) {
        int len = _snprintf_s(out + pos, outcap - pos, _TRUNCATE, "%ld", nums[i]);
        if (len < 0) { /* если переполнение, но заранее выделено много места — на всякий случай */ break; }
        pos += (size_t)len;
        if (i + 1 < cnt) out[pos++] = ' ';
    }

    UnmapViewOfFile(view);
    CloseHandle(hm);

    LARGE_INTEGER newSize; newSize.QuadPart = (LONGLONG)pos;
    if (!SetFilePointerEx(hf, newSize, NULL, FILE_BEGIN) || !SetEndOfFile(hf)) {
        printf("Ошибка установки размера файла: %lu\n", GetLastError());
        CloseHandle(hf);
        free(buf); free(out); free(nums);
        return 1;
    }

    HANDLE hm2 = CreateFileMappingA(hf, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (!hm2) { printf("CreateFileMapping failed: %lu\n", GetLastError()); CloseHandle(hf); free(buf); free(out); free(nums); return 1; }
    char* view2 = (char*)MapViewOfFile(hm2, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!view2) { printf("MapViewOfFile failed: %lu\n", GetLastError()); CloseHandle(hm2); CloseHandle(hf); free(buf); free(out); free(nums); return 1; }

    memcpy(view2, out, pos);

    UnmapViewOfFile(view2);
    CloseHandle(hm2);
    CloseHandle(hf);

    free(buf);
    free(out);
    free(nums);

    printf("Отсортировано %zu чисел по убыванию и записано обратно.\n", cnt);
    return 0;
}

void interactive_loop() {
    char line[512];
    while (1) {
        printf("mapping> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        char *p = line; while (*p && *p != '\n' && *p != '\r') ++p; *p = 0;
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;
        if (strlen(line) == 0) continue;
        if (strcmp(line, "help") == 0) { print_help(); continue; }
        char cmd[64], arg[420];
        int n = sscanf(line, "%63s %419[^\n]", cmd, arg);
        if (n >= 1) {
            if (strcmp(cmd, "sortletters") == 0 && n == 2) cmd_sortletters(arg);
            else if (strcmp(cmd, "countcase") == 0 && n == 2) cmd_countcase(arg);
            else if (strcmp(cmd, "removea") == 0 && n == 2) cmd_removea(arg);
            else if (strcmp(cmd, "sortnums") == 0 && n == 2) cmd_sortnums(arg);
            else printf("Неизвестная команда или неверные аргументы. Введите help.\n");
        }
    }
}

int main(int argc, char** argv) {
    if (argc == 1) { printf("Интерактивный режим. Введите help.\n"); interactive_loop(); return 0; }
    if (argc >= 2) {
        const char* cmd = argv[1];
        if (strcmp(cmd, "help") == 0) { print_help(); return 0; }
        if (strcmp(cmd, "sortletters") == 0 && argc == 3) return cmd_sortletters(argv[2]);
        if (strcmp(cmd, "countcase") == 0 && argc == 3) return cmd_countcase(argv[2]);
        if (strcmp(cmd, "removea") == 0 && argc == 3) return cmd_removea(argv[2]);
        if (strcmp(cmd, "sortnums") == 0 && argc == 3) return cmd_sortnums(argv[2]);
        printf("Неверная команда или аргументы. mapping_tool.exe help\n");
    }
    return 0;
}
