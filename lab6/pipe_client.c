#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>

#define PIPE_BUFSIZE 4096

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s \\\\.\\pipe\\pipename\n", argv[0]);
        return 1;
    }
    const char *pipename = argv[1];
    HANDLE hPipe;

    while (1) {
        hPipe = CreateFileA(
            pipename,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
        if (hPipe != INVALID_HANDLE_VALUE) break;
        if (GetLastError() != ERROR_PIPE_BUSY) {
            printf("Could not open pipe. GLE=%lu\n", GetLastError());
            return 1;
        }
        if (!WaitNamedPipeA(pipename, 5000)) {
            printf("Timed out waiting for pipe\n"); return 1;
        }
    }

    printf("Connected to pipe %s\n", pipename);

    char input[PIPE_BUFSIZE];
    char buf[PIPE_BUFSIZE];
    DWORD written, read;

    while (fgets(input, sizeof(input), stdin)) {
        size_t L = strlen(input);
        if (L > 0 && input[L-1] == '\n') input[L-1] = 0;
        if (strcmp(input, "/quit") == 0) break;

        BOOL ok = WriteFile(hPipe, input, (DWORD)strlen(input), &written, NULL);
        if (!ok) { printf("WriteFile failed: %lu\n", GetLastError()); break; }

        ok = ReadFile(hPipe, buf, sizeof(buf)-1, &read, NULL);
        if (!ok || read == 0) { printf("ReadFile failed or zero: %lu\n", GetLastError()); break; }
        buf[read] = 0;
        printf("Echo: %s\n", buf);
    }

    CloseHandle(hPipe);
    return 0;
}
