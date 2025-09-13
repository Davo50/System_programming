/*
 lab3_task2.c
 Лабораторная работа 3 — Задание 2 (mapping файлов через WinAPI)
 Поддерживает команды:
   sortletters <file>   - упорядочить буквы по алфавиту, заменив буквы в файле на отсортированные (в исходных буквенных позициях)
   countcases <file>    - вывести количество строчных и прописных букв
   removea <file>       - удалить все буквы 'a' и 'A' из текста; в конец файла дописать число удаленных символов
   sortnum <file>       - считать числа (целые) из файла, отсортировать по убыванию и перезаписать файл числами, разделенными пробелом
   (без args) интерактивный режим, help
*/
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* helper to print last error */
static void print_last_error(const char *pref) {
    DWORD e = GetLastError();
    char *msg = NULL;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, e, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msg, 0, NULL);
    if (msg) {
        printf("%s: %s", pref, msg);
        LocalFree(msg);
    } else {
        printf("%s: error %u\n", pref, (unsigned)e);
    }
}

/* map entire file (read-write if writable requested) */
static void *map_file(const char *path, HANDLE *outFile, HANDLE *outMap, SIZE_T *outSize, int writable) {
    DWORD access = writable ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ;
    HANDLE h = CreateFileA(path, access, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { print_last_error("CreateFile"); return NULL; }
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz)) { print_last_error("GetFileSizeEx"); CloseHandle(h); return NULL; }
    SIZE_T size = (SIZE_T)sz.QuadPart;
    DWORD protect = writable ? PAGE_READWRITE : PAGE_READONLY;
    HANDLE map = CreateFileMappingA(h, NULL, protect, 0, 0, NULL);
    if (!map) { print_last_error("CreateFileMapping"); CloseHandle(h); return NULL; }
    DWORD desired = writable ? FILE_MAP_WRITE : FILE_MAP_READ;
    LPVOID view = MapViewOfFile(map, desired, 0, 0, 0);
    if (!view) { print_last_error("MapViewOfFile"); CloseHandle(map); CloseHandle(h); return NULL; }
    if (outFile) *outFile = h; else CloseHandle(h); /* if caller wants handle, pass it */
    if (outMap) *outMap = map; else CloseHandle(map);
    if (outSize) *outSize = size;
    return view;
}

/* unmap with handles */
static void unmap_file(void *view, HANDLE map, HANDLE file) {
    if (view) UnmapViewOfFile(view);
    if (map) CloseHandle(map);
    if (file) CloseHandle(file);
}

/* 1) collect indices of letter positions and letters; sort letters case-insensitive and write back into letter positions (preserve exact case as in sorted order) */
static int cmd_sortletters(const char *path) {
    HANDLE file = NULL, map = NULL;
    SIZE_T size = 0;
    char *view = (char*)map_file(path, &file, &map, &size, 1);
    if (!view) return 1;
    /* collect indices and chars */
    SIZE_T i, count = 0;
    for (i = 0; i < size; ++i) if (isalpha((unsigned char)view[i])) ++count;
    if (count == 0) { printf("No letters found\n"); unmap_file(view, map, file); return 0; }
    char *letters = malloc(count);
    SIZE_T *indices = malloc(count * sizeof(SIZE_T));
    if (!letters || !indices) { fprintf(stderr, "Memory alloc\n"); free(letters); free(indices); unmap_file(view,map,file); return 1; }
    SIZE_T k = 0;
    for (i = 0; i < size; ++i) {
        if (isalpha((unsigned char)view[i])) { letters[k] = view[i]; indices[k] = i; ++k; }
    }
    /* sort letters case-insensitive but stable: we'll use simple qsort with compare */
    int cmp_alpha(const void *a, const void *b) {
        unsigned char A = *(const unsigned char*)a;
        unsigned char B = *(const unsigned char*)b;
        int la = tolower(A), lb = tolower(B);
        if (la != lb) return la - lb;
        return A - B;
    }
    qsort(letters, count, 1, cmp_alpha);
    /* write sorted letters back into letter positions */
    for (SIZE_T j = 0; j < count; ++j) view[indices[j]] = letters[j];
    /* flush view to disk */
    if (!FlushViewOfFile(view, 0)) print_last_error("FlushViewOfFile");
    printf("Sorted %u letters in file '%s'\n", (unsigned)count, path);
    free(letters); free(indices);
    unmap_file(view, map, file);
    return 0;
}

/* 2) count lower and upper letters */
static int cmd_countcases(const char *path) {
    HANDLE file = NULL, map = NULL;
    SIZE_T size = 0;
    const char *view = (const char*)map_file(path, &file, &map, &size, 0);
    if (!view) return 1;
    unsigned long long lower = 0, upper = 0;
    for (SIZE_T i = 0; i < size; ++i) {
        unsigned char c = (unsigned char)view[i];
        if (isalpha(c)) {
            if (islower(c)) ++lower; else if (isupper(c)) ++upper;
        }
    }
    printf("File '%s': lower=%llu upper=%llu\n", path, lower, upper);
    unmap_file((void*)view, map, file);
    return 0;
}

/* helper: rewrite file content (truncate) from buffer */
static int rewrite_file_from_buffer(const char *path, const void *buf, SIZE_T len) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE | GENERIC_READ, 0, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { print_last_error("CreateFile (rewrite)"); return 1; }
    DWORD written = 0;
    if (len > 0) {
        if (!WriteFile(h, buf, (DWORD)len, &written, NULL)) { print_last_error("WriteFile (rewrite)"); CloseHandle(h); return 1; }
    }
    /* set file pointer to end to correct size */
    SetFilePointer(h, 0, NULL, FILE_END);
    CloseHandle(h);
    return 0;
}

/* 3) remove all 'a' and 'A', append count at the end of file (as ASCII digits) */
static int cmd_removea(const char *path) {
    HANDLE file = NULL, map = NULL;
    SIZE_T size = 0;
    const char *view = (const char*)map_file(path, &file, &map, &size, 0);
    if (!view) return 1;
    /* build new buffer excluding 'a'/'A' */
    char *buf = malloc(size + 32);
    if (!buf) { fprintf(stderr, "Memory alloc\n"); unmap_file((void*)view, map, file); return 1; }
    SIZE_T k = 0;
    unsigned long long removed = 0;
    for (SIZE_T i = 0; i < size; ++i) {
        unsigned char c = (unsigned char)view[i];
        if (c == 'a' || c == 'A') { ++removed; continue; }
        buf[k++] = view[i];
    }
    /* append decimal count as text with newline */
    int extra = snprintf(buf + k, 32, "\n%llu\n", removed);
    if (extra < 0) extra = 0;
    k += extra;
    unmap_file((void*)view, map, file);
    /* rewrite file */
    int rc = rewrite_file_from_buffer(path, buf, k);
    free(buf);
    if (rc == 0) printf("Removed %llu occurrences of 'a'/'A' and appended count.\n", removed);
    return rc;
}

/* 4) parse integers from file, sort descending, rewrite file with numbers separated by spaces */
static int cmd_sortnum(const char *path) {
    HANDLE file = NULL, map = NULL;
    SIZE_T size = 0;
    const char *view = (const char*)map_file(path, &file, &map, &size, 0);
    if (!view) return 1;
    /* copy to buffer and null-terminate */
    char *buf = malloc(size + 1);
    if (!buf) { fprintf(stderr, "Memory alloc\n"); unmap_file((void*)view, map, file); return 1; }
    memcpy(buf, view, size); buf[size] = 0;
    unmap_file((void*)view, map, file);
    /* parse long long numbers */
    long long *arr = NULL; size_t cnt = 0;
    char *p = buf;
    while (*p) {
        /* skip non-digit and non-sign */
        while (*p && !(isdigit((unsigned char)*p) || *p == '-' || *p == '+')) ++p;
        if (!*p) break;
        char *end;
        long long v = strtoll(p, &end, 10);
        if (end == p) { ++p; continue; }
        long long *tmp = realloc(arr, (cnt + 1) * sizeof(long long));
        if (!tmp) { fprintf(stderr, "Memory alloc\n"); free(arr); free(buf); return 1; }
        arr = tmp; arr[cnt++] = v;
        p = end;
    }
    if (cnt == 0) { printf("No integers found in file.\n"); free(arr); free(buf); return 0; }
    /* sort descending */
    int cmp_desc(const void *a, const void *b) { long long A = *(const long long*)a; long long B = *(const long long*)b; if (A < B) return 1; if (A > B) return -1; return 0; }
    qsort(arr, cnt, sizeof(long long), cmp_desc);
    /* prepare output buffer */
    size_t outcap = cnt * 32;
    char *out = malloc(outcap);
    if (!out) { fprintf(stderr, "Memory alloc\n"); free(arr); free(buf); return 1; }
    size_t off = 0;
    for (size_t i = 0; i < cnt; ++i) {
        int w = snprintf(out + off, outcap - off, "%lld%s", arr[i], (i + 1 < cnt) ? " " : "\n");
        if (w < 0) { fprintf(stderr, "snprintf error\n"); break; }
        off += (size_t)w;
        if (off + 64 > outcap) {
            outcap *= 2;
            char *t = realloc(out, outcap);
            if (!t) { fprintf(stderr, "Memory alloc\n"); break; }
            out = t;
        }
    }
    /* rewrite file */
    int rc = rewrite_file_from_buffer(path, out, off);
    if (rc == 0) printf("Sorted %u numbers and rewrote file '%s'\n", (unsigned)cnt, path);
    free(out); free(arr); free(buf);
    return rc;
}

/* interactive */
static void interactive(void) {
    char line[512];
    while (1) {
        printf("\nlab3_task2> Commands:\n"
               "  sortletters <file>\n"
               "  countcases <file>\n"
               "  removea <file>\n"
               "  sortnum <file>\n"
               "  exit\n"
               "Enter: ");
        if (!fgets(line, sizeof(line), stdin)) break;
        char *p = strchr(line, '\n'); if (p) *p = 0;
        if (strlen(line) == 0) continue;
        char *argv[3]; int argc = 0;
        char *tok = strtok(line, " \t");
        while (tok && argc < 3) { argv[argc++] = tok; tok = strtok(NULL, " \t"); }
        if (argc == 0) continue;
        if (_stricmp(argv[0], "exit") == 0) break;
        if (_stricmp(argv[0], "sortletters") == 0) { if (argc == 2) cmd_sortletters(argv[1]); else printf("Usage: sortletters <file>\n"); }
        else if (_stricmp(argv[0], "countcases") == 0) { if (argc == 2) cmd_countcases(argv[1]); else printf("Usage: countcases <file>\n"); }
        else if (_stricmp(argv[0], "removea") == 0) { if (argc == 2) cmd_removea(argv[1]); else printf("Usage: removea <file>\n"); }
        else if (_stricmp(argv[0], "sortnum") == 0) { if (argc == 2) cmd_sortnum(argv[1]); else printf("Usage: sortnum <file>\n"); }
        else printf("Unknown command\n");
    }
}

static void print_help(void) {
    printf("lab3_task2.exe - memory-mapped file operations\n"
           "Commands:\n"
           "  sortletters <file>\n"
           "  countcases <file>\n"
           "  removea <file>\n"
           "  sortnum <file>\n"
           "Run without args for interactive mode.\n");
}

int main(int argc, char **argv) {
    if (argc == 1) { interactive(); return 0; }
    if (argc >= 2) {
        if (_stricmp(argv[1], "help") == 0 || _stricmp(argv[1], "-h") == 0 || _stricmp(argv[1], "--help") == 0) { print_help(); return 0; }
        if (_stricmp(argv[1], "sortletters") == 0) { if (argc == 3) return cmd_sortletters(argv[2]); fprintf(stderr,"Usage: sortletters <file>\n"); return 1; }
        if (_stricmp(argv[1], "countcases") == 0) { if (argc == 3) return cmd_countcases(argv[2]); fprintf(stderr,"Usage: countcases <file>\n"); return 1; }
        if (_stricmp(argv[1], "removea") == 0) { if (argc == 3) return cmd_removea(argv[2]); fprintf(stderr,"Usage: removea <file>\n"); return 1; }
        if (_stricmp(argv[1], "sortnum") == 0) { if (argc == 3) return cmd_sortnum(argv[2]); fprintf(stderr,"Usage: sortnum <file>\n"); return 1; }
    }
    print_help();
    return 0;
}
