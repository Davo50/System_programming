/*
 lab3_task1.c
 Лабораторная работа 3 — Задание 1
 Утилита для базовых операций с файлами через WinAPI.
 Поддерживает как интерактивный режим (без аргументов), так и
 режим командной строки:
   lab3_task1.exe create <file> [<content>]
   lab3_task1.exe read <file>
   lab3_task1.exe delete <file>
   lab3_task1.exe rename <oldpath> <newpath>
   lab3_task1.exe copy <src> <dst>
   lab3_task1.exe size <file>
   lab3_task1.exe attrs <file>
   lab3_task1.exe setro <file>
   lab3_task1.exe sethidden <file>
   lab3_task1.exe list <directory>
   lab3_task1.exe help
*/

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static void print_last_error(const char *prefix) {
    DWORD e = GetLastError();
    if (e == 0) {
        if (prefix) printf("%s: no error\n", prefix);
        return;
    }
    char *msg = NULL;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, e, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msg, 0, NULL);
    if (msg) {
        if (prefix) printf("%s: %s", prefix, msg);
        LocalFree(msg);
    } else {
        if (prefix) printf("%s: error code %u\n", prefix, (unsigned)e);
    }
}

/* create file and write content (content may be NULL or empty) */
static int cmd_create(const char *path, const char *content) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { print_last_error("CreateFile"); return 1; }
    DWORD written = 0;
    if (content && *content) {
        if (!WriteFile(h, content, (DWORD)strlen(content), &written, NULL)) {
            print_last_error("WriteFile");
            CloseHandle(h);
            return 1;
        }
    }
    CloseHandle(h);
    printf("Created '%s' (%u bytes written)\n", path, (unsigned)written);
    return 0;
}

/* read file and dump to stdout */
static int cmd_read(const char *path) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { print_last_error("CreateFile"); return 1; }
    char buf[4096];
    DWORD r;
    while (ReadFile(h, buf, (DWORD)sizeof(buf), &r, NULL) && r > 0) {
        fwrite(buf, 1, r, stdout);
    }
    printf("\n");
    CloseHandle(h);
    return 0;
}

static int cmd_delete(const char *path) {
    if (!DeleteFileA(path)) { print_last_error("DeleteFile"); return 1; }
    printf("Deleted '%s'\n", path);
    return 0;
}

static int cmd_rename(const char *oldp, const char *newp) {
    if (!MoveFileExA(oldp, newp, MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)) { print_last_error("MoveFileEx"); return 1; }
    printf("Renamed '%s' -> '%s'\n", oldp, newp);
    return 0;
}

static int cmd_copy(const char *src, const char *dst) {
    if (!CopyFileA(src, dst, FALSE)) { print_last_error("CopyFile"); return 1; }
    printf("Copied '%s' -> '%s'\n", src, dst);
    return 0;
}

static int cmd_size(const char *path) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { print_last_error("CreateFile"); return 1; }
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz)) { print_last_error("GetFileSizeEx"); CloseHandle(h); return 1; }
    printf("Size of '%s' = %lld bytes\n", path, (long long)sz.QuadPart);
    CloseHandle(h);
    return 0;
}

static void print_attributes(DWORD attr) {
    if (attr == INVALID_FILE_ATTRIBUTES) { printf("Invalid attributes\n"); return; }
    printf("Attributes:");
    if (attr & FILE_ATTRIBUTE_READONLY) printf(" READONLY");
    if (attr & FILE_ATTRIBUTE_HIDDEN) printf(" HIDDEN");
    if (attr & FILE_ATTRIBUTE_SYSTEM) printf(" SYSTEM");
    if (attr & FILE_ATTRIBUTE_DIRECTORY) printf(" DIRECTORY");
    if (attr & FILE_ATTRIBUTE_ARCHIVE) printf(" ARCHIVE");
    if (attr & FILE_ATTRIBUTE_NORMAL) printf(" NORMAL");
    if (attr & FILE_ATTRIBUTE_TEMPORARY) printf(" TEMPORARY");
    if (attr & FILE_ATTRIBUTE_COMPRESSED) printf(" COMPRESSED");
    if (attr & FILE_ATTRIBUTE_ENCRYPTED) printf(" ENCRYPTED");
    printf("\n");
}

static int cmd_attrs(const char *path) {
    DWORD a = GetFileAttributesA(path);
    if (a == INVALID_FILE_ATTRIBUTES) { print_last_error("GetFileAttributes"); return 1; }
    print_attributes(a);
    return 0;
}

static int cmd_set_attribute(const char *path, DWORD attrflag) {
    DWORD a = GetFileAttributesA(path);
    if (a == INVALID_FILE_ATTRIBUTES) { print_last_error("GetFileAttributes"); return 1; }
    DWORD newa = a | attrflag;
    if (!SetFileAttributesA(path, newa)) { print_last_error("SetFileAttributes"); return 1; }
    printf("Set attribute on '%s'\n", path);
    return 0;
}

/* recursive listing */
static void list_dir_recursive(const char *dir, int level) {
    char pattern[MAX_PATH];
    snprintf(pattern, MAX_PATH, "%s\\*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        for (int i = 0; i < level; ++i) putchar(' ');
        char path[MAX_PATH];
        snprintf(path, MAX_PATH, "%s\\%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            printf("[DIR] %s\n", fd.cFileName);
            /* recurse */
            list_dir_recursive(path, level + 2);
        } else {
            printf("      %s\n", fd.cFileName);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

/* interactive menu */
static void interactive(void) {
    char cmd[256];
    while (1) {
        printf("\n=== lab3_task1 interactive ===\n"
               "Commands:\n"
               "  create <file> [<content>]\n"
               "  read <file>\n"
               "  delete <file>\n"
               "  rename <old> <new>\n"
               "  copy <src> <dst>\n"
               "  size <file>\n"
               "  attrs <file>\n"
               "  setro <file>\n"
               "  sethidden <file>\n"
               "  list <dir>\n"
               "  exit\n"
               "Enter command: ");
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        /* trim newline */
        char *p = strchr(cmd, '\n'); if (p) *p = 0;
        if (strlen(cmd) == 0) continue;
        /* tokenize */
        char *argv[4] = {0};
        int argc = 0;
        char *tok = strtok(cmd, " \t");
        while (tok && argc < 4) { argv[argc++] = tok; tok = strtok(NULL, " \t"); }
        if (argc == 0) continue;
        if (_stricmp(argv[0], "exit") == 0) break;
        if (_stricmp(argv[0], "create") == 0) {
            if (argc >= 2) cmd_create(argv[1], argc >= 3 ? argv[2] : "");
            else printf("Usage: create <file> [<content>]\n");
        } else if (_stricmp(argv[0], "read") == 0) {
            if (argc == 2) cmd_read(argv[1]); else printf("Usage: read <file>\n");
        } else if (_stricmp(argv[0], "delete") == 0) {
            if (argc == 2) cmd_delete(argv[1]); else printf("Usage: delete <file>\n");
        } else if (_stricmp(argv[0], "rename") == 0) {
            if (argc == 3) cmd_rename(argv[1], argv[2]); else printf("Usage: rename <old> <new>\n");
        } else if (_stricmp(argv[0], "copy") == 0) {
            if (argc == 3) cmd_copy(argv[1], argv[2]); else printf("Usage: copy <src> <dst>\n");
        } else if (_stricmp(argv[0], "size") == 0) {
            if (argc == 2) cmd_size(argv[1]); else printf("Usage: size <file>\n");
        } else if (_stricmp(argv[0], "attrs") == 0) {
            if (argc == 2) cmd_attrs(argv[1]); else printf("Usage: attrs <file>\n");
        } else if (_stricmp(argv[0], "setro") == 0) {
            if (argc == 2) cmd_set_attribute(argv[1], FILE_ATTRIBUTE_READONLY); else printf("Usage: setro <file>\n");
        } else if (_stricmp(argv[0], "sethidden") == 0) {
            if (argc == 2) cmd_set_attribute(argv[1], FILE_ATTRIBUTE_HIDDEN); else printf("Usage: sethidden <file>\n");
        } else if (_stricmp(argv[0], "list") == 0) {
            if (argc == 2) list_dir_recursive(argv[1], 0); else printf("Usage: list <dir>\n");
        } else {
            printf("Unknown command. Type 'exit' to quit.\n");
        }
    }
}

/* print cmdline help */
static void print_help(void) {
    printf("lab3_task1.exe - WinAPI file ops\n"
           "Usage:\n"
           "  lab3_task1.exe <command> [args]\n"
           "Commands:\n"
           "  create <file> [<content>]  - create file and write content\n"
           "  read <file>                - read file and print\n"
           "  delete <file>              - delete file\n"
           "  rename <old> <new>         - rename/move file\n"
           "  copy <src> <dst>           - copy file\n"
           "  size <file>                - print file size\n"
           "  attrs <file>               - print file attributes\n"
           "  setro <file>               - set readonly attribute\n"
           "  sethidden <file>           - set hidden attribute\n"
           "  list <directory>           - recursive listing\n"
           "  (no args)                  - interactive mode\n");
}

int main(int argc, char **argv) {
    if (argc == 1) { interactive(); return 0; }
    if (argc >= 2) {
        if (_stricmp(argv[1], "help") == 0 || _stricmp(argv[1], "-h") == 0 || _stricmp(argv[1], "--help") == 0) {
            print_help(); return 0;
        }
        /* commands */
        if (_stricmp(argv[1], "create") == 0) {
            if (argc >= 3) return cmd_create(argv[2], argc >= 4 ? argv[3] : "");
            fprintf(stderr, "Usage: create <file> [<content>]\n"); return 1;
        } else if (_stricmp(argv[1], "read") == 0) {
            if (argc == 3) return cmd_read(argv[2]); fprintf(stderr, "Usage: read <file>\n"); return 1;
        } else if (_stricmp(argv[1], "delete") == 0) {
            if (argc == 3) return cmd_delete(argv[2]); fprintf(stderr, "Usage: delete <file>\n"); return 1;
        } else if (_stricmp(argv[1], "rename") == 0) {
            if (argc == 4) return cmd_rename(argv[2], argv[3]); fprintf(stderr, "Usage: rename <old> <new>\n"); return 1;
        } else if (_stricmp(argv[1], "copy") == 0) {
            if (argc == 4) return cmd_copy(argv[2], argv[3]); fprintf(stderr, "Usage: copy <src> <dst>\n"); return 1;
        } else if (_stricmp(argv[1], "size") == 0) {
            if (argc == 3) return cmd_size(argv[2]); fprintf(stderr, "Usage: size <file>\n"); return 1;
        } else if (_stricmp(argv[1], "attrs") == 0) {
            if (argc == 3) return cmd_attrs(argv[2]); fprintf(stderr, "Usage: attrs <file>\n"); return 1;
        } else if (_stricmp(argv[1], "setro") == 0) {
            if (argc == 3) return cmd_set_attribute(argv[2], FILE_ATTRIBUTE_READONLY); fprintf(stderr, "Usage: setro <file>\n"); return 1;
        } else if (_stricmp(argv[1], "sethidden") == 0) {
            if (argc == 3) return cmd_set_attribute(argv[2], FILE_ATTRIBUTE_HIDDEN); fprintf(stderr, "Usage: sethidden <file>\n"); return 1;
        } else if (_stricmp(argv[1], "list") == 0) {
            if (argc == 3) { list_dir_recursive(argv[2], 0); return 0; } fprintf(stderr, "Usage: list <dir>\n"); return 1;
        }
    }
    print_help();
    return 0;
}
