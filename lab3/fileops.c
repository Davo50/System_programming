#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_help() {
    printf("fileops.exe - утилита для базовых операций с файлами (WinAPI)\n");
    printf("Запуск без аргументов — интерактивный режим.\n");
    printf("Команды (через аргументы):\n");
    printf("  create <path> <content>    - создать файл и записать содержимое\n");
    printf("  read <path>                - вывести содержимое файла\n");
    printf("  delete <path>              - удалить файл\n");
    printf("  rename <old> <new>         - переименовать файл\n");
    printf("  copy <src> <dst>          - копировать файл\n");
    printf("  size <path>               - вывести размер файла (в байтах)\n");
    printf("  attr <path>               - вывести атрибуты файла (READONLY, HIDDEN, ...)\n");
    printf("  setreadonly <path> <0|1>  - снять/установить атрибут только для чтения\n");
    printf("  sethidden <path> <0|1>    - снять/установить атрибут скрытый\n");
    printf("  list <folder>             - рекурсивно перечислить папку\n");
    printf("  help                      - это сообщение\n");
}

void create_and_write(const char* path, const char* content) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("Ошибка CreateFile: %lu\n", GetLastError());
        return;
    }
    DWORD written;
    if (!WriteFile(h, content, (DWORD)strlen(content), &written, NULL)) {
        printf("Ошибка WriteFile: %lu\n", GetLastError());
    } else {
        printf("Записано %lu байт в %s\n", written, path);
    }
    CloseHandle(h);
}

// Обновлённая функция: чтение файла любого размера (потоковое чтение по кускам)
void read_and_print(const char* path) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("Ошибка CreateFile (open): %lu\n", GetLastError());
        return;
    }

    LARGE_INTEGER szli;
    BOOL gotSize = GetFileSizeEx(h, &szli);

    printf("---- содержимое %s ----\n", path);

    const DWORD CHUNK = 64 * 1024; // 64KB
    char* buf = (char*)malloc(CHUNK);
    if (!buf) {
        printf("Нет памяти\n");
        CloseHandle(h);
        return;
    }

    if (gotSize) {
        // Чтение известного размера файла по частям (поддерживает >2GB)
        unsigned long long remaining = (unsigned long long)szli.QuadPart;
        if (remaining == 0) {
            printf("[пустой файл]\n");
            free(buf);
            CloseHandle(h);
            return;
        }
        while (remaining > 0) {
            DWORD toRead = (DWORD)(remaining > CHUNK ? CHUNK : remaining);
            DWORD read = 0;
            if (!ReadFile(h, buf, toRead, &read, NULL)) {
                printf("\nОшибка ReadFile: %lu\n", GetLastError());
                break;
            }
            if (read == 0) break; // EOF
            size_t wrote = fwrite(buf, 1, read, stdout);
            (void)wrote;
            remaining -= read;
        }
    } else {
        // Если размер получить не удалось (например, специальный файл/пайп), читаем, пока ReadFile возвращает данные
        DWORD read = 0;
        while (1) {
            if (!ReadFile(h, buf, CHUNK, &read, NULL)) {
                DWORD err = GetLastError();
                if (err == ERROR_HANDLE_EOF || read == 0) break;
                printf("\nОшибка ReadFile: %lu\n", err);
                break;
            }
            if (read == 0) break;
            size_t wrote = fwrite(buf, 1, read, stdout);
            (void)wrote;
        }
    }

    printf("\n---- конец ----\n");
    free(buf);
    CloseHandle(h);
}

void delete_file_cmd(const char* path) {
    if (!DeleteFileA(path)) {
        printf("Ошибка DeleteFile: %lu\n", GetLastError());
    } else {
        printf("Файл %s удалён\n", path);
    }
}

void rename_file_cmd(const char* oldn, const char* newn) {
    if (!MoveFileA(oldn, newn)) {
        printf("Ошибка MoveFile: %lu\n", GetLastError());
    } else {
        printf("Переименовано %s -> %s\n", oldn, newn);
    }
}

void copy_file_cmd(const char* src, const char* dst) {
    if (!CopyFileA(src, dst, FALSE)) {
        printf("Ошибка CopyFile: %lu\n", GetLastError());
    } else {
        printf("Скопировано %s -> %s\n", src, dst);
    }
}

void print_size(const char* path) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { printf("Ошибка CreateFile: %lu\n", GetLastError()); return; }
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz)) { printf("Ошибка GetFileSizeEx: %lu\n", GetLastError()); CloseHandle(h); return; }
    printf("Размер %s = %lld байт\n", path, (long long)sz.QuadPart);
    CloseHandle(h);
}

void print_attributes(const char* path) {
    DWORD a = GetFileAttributesA(path);
    if (a == INVALID_FILE_ATTRIBUTES) { printf("Ошибка GetFileAttributes: %lu\n", GetLastError()); return; }
    printf("Атрибуты для %s:\n", path);
    if (a & FILE_ATTRIBUTE_ARCHIVE) printf("  ARCHIVE\n");
    if (a & FILE_ATTRIBUTE_COMPRESSED) printf("  COMPRESSED\n");
    if (a & FILE_ATTRIBUTE_DIRECTORY) printf("  DIRECTORY\n");
    if (a & FILE_ATTRIBUTE_HIDDEN) printf("  HIDDEN\n");
    if (a & FILE_ATTRIBUTE_NORMAL) printf("  NORMAL\n");
    if (a & FILE_ATTRIBUTE_READONLY) printf("  READONLY\n");
    if (a & FILE_ATTRIBUTE_SYSTEM) printf("  SYSTEM\n");
    if (a & FILE_ATTRIBUTE_TEMPORARY) printf("  TEMPORARY\n");

}

void set_readonly(const char* path, int on) {
    DWORD a = GetFileAttributesA(path);
    if (a == INVALID_FILE_ATTRIBUTES) { printf("Ошибка GetFileAttributes: %lu\n", GetLastError()); return; }
    if (on) a |= FILE_ATTRIBUTE_READONLY; else a &= ~FILE_ATTRIBUTE_READONLY;
    if (!SetFileAttributesA(path, a)) printf("Ошибка SetFileAttributes: %lu\n", GetLastError());
    else printf("Атрибут READONLY для %s %s\n", path, on ? "установлен" : "снят");
}

void set_hidden(const char* path, int on) {
    DWORD a = GetFileAttributesA(path);
    if (a == INVALID_FILE_ATTRIBUTES) { printf("Ошибка GetFileAttributes: %lu\n", GetLastError()); return; }
    if (on) a |= FILE_ATTRIBUTE_HIDDEN; else a &= ~FILE_ATTRIBUTE_HIDDEN;
    if (!SetFileAttributesA(path, a)) printf("Ошибка SetFileAttributes: %lu\n", GetLastError());
    else printf("Атрибут HIDDEN для %s %s\n", path, on ? "установлен" : "снят");
}

void list_folder_recursive(const char* folder, int level) {
    char searchPath[MAX_PATH];
    snprintf(searchPath, MAX_PATH, "%s\\*", folder);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(searchPath, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        for (int i=0;i<level;i++) printf("  ");
        printf("%s", fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            printf(" [DIR]\n");
            char next[MAX_PATH];
            snprintf(next, MAX_PATH, "%s\\%s", folder, fd.cFileName);
            list_folder_recursive(next, level+1);
        } else {
            LARGE_INTEGER sz;
            char fpath[MAX_PATH];
            snprintf(fpath, MAX_PATH, "%s\\%s", folder, fd.cFileName);
            HANDLE hf = CreateFileA(fpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hf != INVALID_HANDLE_VALUE) {
                if (!GetFileSizeEx(hf, &sz)) sz.QuadPart = -1;
                CloseHandle(hf);
            } else sz.QuadPart = -1;
            if (sz.QuadPart >= 0) printf("  [%lld bytes]\n", (long long)sz.QuadPart); else printf("\n");
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

void interactive_loop() {
    char cmd[256];
    while (1) {
        printf("\nfileops> ");
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        char* p = cmd;
        while(*p && *p!='\n' && *p!='\r') p++;
        *p = 0;
        if (strlen(cmd)==0) continue;
        if (strcmp(cmd, "exit")==0 || strcmp(cmd, "quit")==0) break;
        if (strcmp(cmd, "help")==0) { print_help(); continue; }
        char *args[4]; int cnt=0;
        char *tok = strtok(cmd, " ");
        while (tok && cnt<4) { args[cnt++]=tok; tok=strtok(NULL,""); if (cnt==1 && tok) { /* keep rest as one arg */ break; } }
        if (cnt==0) continue;
        if (strcmp(args[0],"create")==0 && cnt>=2) {
            char *content = "";
            if (cnt==2) content = "";
            if (cnt==3) content = args[2];
            create_and_write(args[1], content);
        } else if (strcmp(args[0],"read")==0 && cnt==2) read_and_print(args[1]);
        else if (strcmp(args[0],"delete")==0 && cnt==2) delete_file_cmd(args[1]);
        else if (strcmp(args[0],"rename")==0 && cnt==3) rename_file_cmd(args[1], args[2]);
        else if (strcmp(args[0],"copy")==0 && cnt==3) copy_file_cmd(args[1], args[2]);
        else if (strcmp(args[0],"size")==0 && cnt==2) print_size(args[1]);
        else if (strcmp(args[0],"attr")==0 && cnt==2) print_attributes(args[1]);
        else if (strcmp(args[0],"setreadonly")==0 && cnt==3) set_readonly(args[1], atoi(args[2]));
        else if (strcmp(args[0],"sethidden")==0 && cnt==3) set_hidden(args[1], atoi(args[2]));
        else if (strcmp(args[0],"list")==0 && cnt==2) list_folder_recursive(args[1], 0);
        else printf("Неизвестная команда или неверное число аргументов. Введите help.\n");
    }
}

int main(int argc, char** argv) {
    if (argc == 1) {
        printf("Интерактивный режим. Введите help для списка команд, exit для выхода.\n");
        interactive_loop();
        return 0;
    }
    if (argc >= 2) {
        const char* cmd = argv[1];
        if (strcmp(cmd,"help")==0) { print_help(); return 0; }
        if (strcmp(cmd,"create")==0 && argc>=4) { create_and_write(argv[2], argv[3]); return 0; }
        if (strcmp(cmd,"read")==0 && argc==3) { read_and_print(argv[2]); return 0; }
        if (strcmp(cmd,"delete")==0 && argc==3) { delete_file_cmd(argv[2]); return 0; }
        if (strcmp(cmd,"rename")==0 && argc==4) { rename_file_cmd(argv[2], argv[3]); return 0; }
        if (strcmp(cmd,"copy")==0 && argc==4) { copy_file_cmd(argv[2], argv[3]); return 0; }
        if (strcmp(cmd,"size")==0 && argc==3) { print_size(argv[2]); return 0; }
        if (strcmp(cmd,"attr")==0 && argc==3) { print_attributes(argv[2]); return 0; }
        if (strcmp(cmd,"setreadonly")==0 && argc==4) { set_readonly(argv[2], atoi(argv[3])); return 0; }
        if (strcmp(cmd,"sethidden")==0 && argc==4) { set_hidden(argv[2], atoi(argv[3])); return 0; }
        if (strcmp(cmd,"list")==0 && argc==3) { list_folder_recursive(argv[2], 0); return 0; }
        printf("Неверная команда или аргументы. Введите fileops.exe help\n");
    }
    return 0;
}
