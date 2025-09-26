#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static SRWLOCK srw;
static volatile LONG stop_flag = 0;
static int OPS_EACH = 10;

void safe_append_to_file(const char* path, const char* buf, DWORD len) {
    HANDLE h = CreateFileA(path, FILE_GENERIC_WRITE | FILE_GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("[Writer] CreateFile failed: %lu\n", GetLastError());
        return;
    }
    LARGE_INTEGER zero = {0};
    LARGE_INTEGER end;
    SetFilePointerEx(h, zero, &end, FILE_END);
    DWORD written;
    WriteFile(h, buf, len, &written, NULL);
    CloseHandle(h);
}

char* safe_read_file(const char* path, DWORD* outSize) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        *outSize = 0;
        return NULL;
    }
    DWORD size = GetFileSize(h, NULL);
    char* buf = (char*)malloc(size + 1);
    DWORD read = 0;
    ReadFile(h, buf, size, &read, NULL);
    buf[read] = '\0';
    *outSize = read;
    CloseHandle(h);
    return buf;
}

DWORD WINAPI Writer(LPVOID arg) {
    int id = (int)(intptr_t)arg;
    for (int i = 0; i < OPS_EACH; ++i) {
        AcquireSRWLockExclusive(&srw);
        char line[256];
        int len = snprintf(line, sizeof(line), "[Writer %d] write #%d\r\n", id, i+1);
        safe_append_to_file("rw_output.txt", line, (DWORD)len);
        printf("[Writer %d] wrote to file: %s", id, line);
        ReleaseSRWLockExclusive(&srw);
        Sleep((rand()%300)+100);
    }
    return 0;
}

DWORD WINAPI Reader(LPVOID arg) {
    int id = (int)(intptr_t)arg;
    for (int i = 0; i < OPS_EACH; ++i) {
        AcquireSRWLockShared(&srw);
        DWORD sz;
        char* content = safe_read_file("rw_output.txt", &sz);
        if (content) {
            printf("  [Reader %d] read %lu bytes (preview): %.60s%s\n", id, (unsigned long)sz, content, sz>60?"...":"");
            free(content);
        } else {
            printf("  [Reader %d] file not found or empty\n", id);
        }
        ReleaseSRWLockShared(&srw);
        Sleep((rand()%250)+50);
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        printf("Usage: %s <num_readers> <num_writers> <filepath> [ops_each]\n", argv[0]);
        return 1;
    }
    int nr = atoi(argv[1]);
    int nw = atoi(argv[2]);
    const char* path = argv[3];
    if (argc >= 5) OPS_EACH = atoi(argv[4]);
    srand((unsigned)GetTickCount());
    InitializeSRWLock(&srw);

    // ensure file exists
    HANDLE h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);

    int total = nr + nw;
    HANDLE* ph = (HANDLE*)malloc(sizeof(HANDLE)*total);
    int idx = 0;
    for (int i = 0; i < nr; ++i) ph[idx++] = CreateThread(NULL,0, Reader, (LPVOID)(intptr_t)(i+1),0,NULL);
    for (int i = 0; i < nw; ++i) ph[idx++] = CreateThread(NULL,0, Writer, (LPVOID)(intptr_t)(i+1),0,NULL);

    WaitForMultipleObjects(total, ph, TRUE, INFINITE);
    for (int i = 0; i < total; ++i) CloseHandle(ph[i]);
    free(ph);

    printf("Readers and Writers finished. Check file: %s\n", path);
    return 0;
}
