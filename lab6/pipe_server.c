#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>

#define PIPE_BUFSIZE 4096

typedef struct {
    HANDLE hPipe;
    int id;
} PIPE_CTX;

DWORD WINAPI client_thread(LPVOID arg) {
    PIPE_CTX *ctx = (PIPE_CTX*)arg;
    HANDLE hPipe = ctx->hPipe;
    char buf[PIPE_BUFSIZE];
    DWORD read;
    printf("Pipe client %d connected\n", ctx->id);

    while (1) {
        BOOL ok = ReadFile(hPipe, buf, sizeof(buf), &read, NULL);
        if (!ok || read == 0) break;
        // echo back
        DWORD written;
        BOOL w = WriteFile(hPipe, buf, read, &written, NULL);
        if (!w || written != read) break;
    }

    printf("Pipe client %d disconnected\n", ctx->id);
    FlushFileBuffers(hPipe);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
    HeapFree(GetProcessHeap(), 0, ctx);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s \\\\.\\pipe\\pipename\n", argv[0]);
        return 1;
    }
    const char *pipename = argv[1];
    printf("Named pipe server starting on %s\n", pipename);

    int client_id = 0;
    while (1) {
        HANDLE hPipe = CreateNamedPipeA(
            pipename,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            PIPE_BUFSIZE, PIPE_BUFSIZE,
            0,
            NULL
        );
        if (hPipe == INVALID_HANDLE_VALUE) {
            printf("CreateNamedPipe failed: %lu\n", GetLastError());
            return 1;
        }
        BOOL connected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (connected) {
            PIPE_CTX *ctx = (PIPE_CTX*)HeapAlloc(GetProcessHeap(), 0, sizeof(PIPE_CTX));
            ctx->hPipe = hPipe;
            ctx->id = ++client_id;
            DWORD tid;
            CreateThread(NULL, 0, client_thread, ctx, 0, &tid);
        } else {
            CloseHandle(hPipe);
        }
    }
    return 0;
}
