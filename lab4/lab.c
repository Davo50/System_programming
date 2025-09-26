#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

typedef LONG NTSTATUS;
typedef NTSTATUS (NTAPI *PFN_NTQUERYINFORMATIONPROCESS)(
    HANDLE ProcessHandle,
    ULONG  ProcessInformationClass,
    PVOID  ProcessInformation,
    ULONG  ProcessInformationLength,
    PULONG ReturnLength
);

typedef struct _PROCESS_BASIC_INFORMATION {
    PVOID Reserved1;
    PVOID PebBaseAddress;
    PVOID Reserved2[2];
    ULONG_PTR UniqueProcessId;
    PVOID Reserved3;
} PROCESS_BASIC_INFORMATION;

typedef struct _UNICODE_STRING_INTERNAL {
    USHORT Length;
    USHORT MaximumLength;
    PVOID  Buffer;
} UNICODE_STRING_INTERNAL;

typedef struct _LDR_DATA_TABLE_ENTRY_INTERNAL {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING_INTERNAL FullDllName;
    UNICODE_STRING_INTERNAL BaseDllName;
} LDR_DATA_TABLE_ENTRY_INTERNAL;

typedef struct _PEB_LDR_DATA_INTERNAL {
    ULONG Length;
    BOOLEAN Initialized;
    PVOID SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
} PEB_LDR_DATA_INTERNAL;

typedef struct _PEB_INTERNAL {
    BYTE Reserved1[2];
    BYTE BeingDebugged;
    BYTE Reserved2[1];
    PVOID Reserved3[2];
    PEB_LDR_DATA_INTERNAL *Ldr;
} PEB_INTERNAL;

static void print_usage(const char *prog) {
    printf("Usage:\n");
    printf("  %s createproc <command line>    - create process and wait\n", prog);
    printf("  %s listproc                     - list running processes (PID + name)\n", prog);
    printf("  %s createthreads [N]            - create N threads (default 5)\n", prog);
    printf("  %s showmodules                  - show modules loaded in current process (via PEB)\n", prog);
}

/* Task 1: Create process and wait */
static int cmd_createproc(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "createproc: missing command line\n");
        return 1;
    }

    /* Reconstruct command line from argv[1..] */
    size_t len = 0;
    for (int i = 1; i < argc; ++i) len += strlen(argv[i]) + 1;
    char *cmd = (char*)malloc(len + 1);
    if (!cmd) return 2;
    cmd[0] = '\0';
    for (int i = 1; i < argc; ++i) {
        strcat(cmd, argv[i]);
        if (i + 1 < argc) strcat(cmd, " ");
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, FALSE,
                             CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
    if (!ok) {
        fprintf(stderr, "CreateProcess failed: %lu\n", GetLastError());
        free(cmd);
        return 3;
    }

    printf("Created process PID=%lu, waiting for exit...\n", (unsigned long)pi.dwProcessId);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitcode = 0;
    if (GetExitCodeProcess(pi.hProcess, &exitcode)) {
        printf("Process exited with code: %lu\n", (unsigned long)exitcode);
    } else {
        fprintf(stderr, "GetExitCodeProcess failed: %lu\n", GetLastError());
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    free(cmd);
    return 0;
}

/* Task 2: List processes */
static int cmd_listproc(void) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "CreateToolhelp32Snapshot failed: %lu\n", GetLastError());
        return 1;
    }

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    if (!Process32First(snap, &pe)) {
        fprintf(stderr, "Process32First failed: %lu\n", GetLastError());
        CloseHandle(snap);
        return 2;
    }

    printf("PID\tProcess Name\n");
    do {
        printf("%lu\t%s\n", (unsigned long)pe.th32ProcessID, pe.szExeFile);
    } while (Process32Next(snap, &pe));

    CloseHandle(snap);
    return 0;
}

/* Thread function */
static DWORD WINAPI simple_thread_func(LPVOID lpParam) {
    int id = (int)(intptr_t)lpParam;
    for (int i = 0; i < 5; ++i) {
        printf("Thread %d: meow (%d)\n", id, i+1);
        Sleep(1000);
    }
    return 0;
}

/* Task 3: Create threads and wait */
static int cmd_createthreads(int argc, char **argv) {
    int n = 5;
    if (argc >= 2) n = atoi(argv[1]);
    if (n <= 0) n = 5;
    if (n > 64) n = 64;

    HANDLE *threads = (HANDLE*)malloc(sizeof(HANDLE) * n);
    if (!threads) return 1;

    for (int i = 0; i < n; ++i) {
        threads[i] = CreateThread(NULL, 0, simple_thread_func, (LPVOID)(intptr_t)(i+1), 0, NULL);
        if (!threads[i]) {
            fprintf(stderr, "CreateThread failed for %d: %lu\n", i+1, GetLastError());
            for (int j = 0; j < i; ++j) CloseHandle(threads[j]);
            free(threads);
            return 2;
        }
    }

    WaitForMultipleObjects(n, threads, TRUE, INFINITE);
    for (int i = 0; i < n; ++i) CloseHandle(threads[i]);
    free(threads);
    printf("All threads finished.\n");
    return 0;
}

/* Read memory helper that works for same-process and remote */
static BOOL read_mem(HANDLE hProc, LPCVOID addr, PVOID buf, SIZE_T size) {
    SIZE_T read = 0;
    /* GetCurrentProcess() returns pseudo-handle; compare values */
    if (hProc == GetCurrentProcess()) {
        memcpy(buf, addr, size);
        return TRUE;
    }
    return ReadProcessMemory(hProc, addr, buf, size, &read) && read == size;
}

/* Task 4: Show modules via PEB */
static int cmd_showmodules(void) {
    HMODULE ntdll = LoadLibraryA("ntdll.dll");
    if (!ntdll) {
        fprintf(stderr, "Failed to load ntdll.dll: %lu\n", GetLastError());
        return 1;
    }
    PFN_NTQUERYINFORMATIONPROCESS NtQueryInformationProcess = (PFN_NTQUERYINFORMATIONPROCESS)GetProcAddress(ntdll, "NtQueryInformationProcess");
    if (!NtQueryInformationProcess) {
        fprintf(stderr, "GetProcAddress(NtQueryInformationProcess) failed\n");
        FreeLibrary(ntdll);
        return 2;
    }

    PROCESS_BASIC_INFORMATION pbi;
    ZeroMemory(&pbi, sizeof(pbi));
    NTSTATUS st = NtQueryInformationProcess(GetCurrentProcess(), 0 /* ProcessBasicInformation */, &pbi, sizeof(pbi), NULL);
    if (st != 0) {
        fprintf(stderr, "NtQueryInformationProcess failed: 0x%lx\n", (unsigned long)st);
        FreeLibrary(ntdll);
        return 3;
    }

    /* Read PEB from current process memory */
    PEB_INTERNAL peb_local;
    if (!read_mem(GetCurrentProcess(), pbi.PebBaseAddress, &peb_local, sizeof(peb_local))) {
        fprintf(stderr, "Failed to read PEB\n");
        FreeLibrary(ntdll);
        return 4;
    }

    /* Address of PEB_LDR_DATA in target process address space */
    PBYTE ldr_addr = (PBYTE)(peb_local.Ldr);
    if (ldr_addr == NULL) {
        fprintf(stderr, "PEB->Ldr is NULL\n");
        FreeLibrary(ntdll);
        return 5;
    }

    /* Address of InLoadOrderModuleList head (address within target process) */
    PBYTE head_addr = ldr_addr + offsetof(PEB_LDR_DATA_INTERNAL, InLoadOrderModuleList);

    /* Read head LIST_ENTRY */
    LIST_ENTRY headEntry;
    if (!read_mem(GetCurrentProcess(), head_addr, &headEntry, sizeof(headEntry))) {
        fprintf(stderr, "Failed to read InLoadOrderModuleList head\n");
        FreeLibrary(ntdll);
        return 6;
    }

    printf("Loaded modules (BaseDllName) for current process:\n");

    PBYTE flinkAddr = (PBYTE)headEntry.Flink;
    while (flinkAddr != NULL && flinkAddr != head_addr) {
        PBYTE entryAddr = flinkAddr - offsetof(LDR_DATA_TABLE_ENTRY_INTERNAL, InLoadOrderLinks);

        LDR_DATA_TABLE_ENTRY_INTERNAL loaded;
        if (!read_mem(GetCurrentProcess(), entryAddr, &loaded, sizeof(loaded))) break;

        if (loaded.BaseDllName.Length > 0 && loaded.BaseDllName.Buffer) {
            SIZE_T bufBytes = loaded.BaseDllName.Length;
            wchar_t *wbuf = (wchar_t*)malloc(bufBytes + sizeof(wchar_t));
            if (wbuf) {
                if (read_mem(GetCurrentProcess(), loaded.BaseDllName.Buffer, wbuf, bufBytes)) {
                    wbuf[bufBytes / sizeof(wchar_t)] = L'\0';
                    int need = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, NULL, 0, NULL, NULL);
                    if (need > 0) {
                        char *out = (char*)malloc(need);
                        if (out) {
                            WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, out, need, NULL, NULL);
                            printf("  %s\n", out);
                            free(out);
                        }
                    }
                }
                free(wbuf);
            }
        }

        flinkAddr = (PBYTE)loaded.InLoadOrderLinks.Flink;
    }

    FreeLibrary(ntdll);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (_stricmp(argv[1], "createproc") == 0) {
        return cmd_createproc(argc - 1, &argv[1]);
    } else if (_stricmp(argv[1], "listproc") == 0) {
        return cmd_listproc();
    } else if (_stricmp(argv[1], "createthreads") == 0) {
        return cmd_createthreads(argc - 1, &argv[1]);
    } else if (_stricmp(argv[1], "showmodules") == 0) {
        return cmd_showmodules();
    } else {
        print_usage(argv[0]);
        return 2;
    }
}
