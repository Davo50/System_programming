#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>

volatile LONG c1 = 0, c2 = 0, c3 = 0;
HANDLE hThread1 = NULL, hThread2 = NULL, hThread3 = NULL;

DWORD WINAPI ThreadFunc1(LPVOID arg) {
    while (1) {
        InterlockedIncrement(&c1);
        Sleep(2);
    }
    return 0;
}
DWORD WINAPI ThreadFunc2(LPVOID arg) {
    while (1) {
        InterlockedIncrement(&c2);
        Sleep(5);
    }
    return 0;
}
DWORD WINAPI ThreadFunc3(LPVOID arg) {
    while (1) {
        InterlockedIncrement(&c3);
        Sleep(1);
    }
    return 0;
}

int main(void) {
    hThread1 = CreateThread(NULL,0,ThreadFunc1,NULL,0,NULL);
    hThread2 = CreateThread(NULL,0,ThreadFunc2,NULL,0,NULL);
    hThread3 = CreateThread(NULL,0,ThreadFunc3,NULL,0,NULL);

    printf("Threads started. Monitoring product of c1*c3.\n");
    while (1) {
        LONG local1 = InterlockedCompareExchange(&c1,0,0);
        LONG local3 = InterlockedCompareExchange(&c3,0,0);
        long product = (long)local1 * (long)local3;
        if (product >= 5000) {
            printf("Product reached %ld (c1=%ld, c3=%ld). Suspending thread 2.\n", product, local1, local3);
            SuspendThread(hThread2);
            printf("Thread 2 suspended. c2=%ld\n", InterlockedCompareExchange(&c2,0,0));
            break;
        }
        Sleep(50);
    }

    printf("Press Enter to terminate program (threads will be terminated)...\n");
    getchar();
    // Terminate threads (for lab/demo only)
    TerminateThread(hThread1, 0);
    TerminateThread(hThread2, 0);
    TerminateThread(hThread3, 0);
    CloseHandle(hThread1);
    CloseHandle(hThread2);
    CloseHandle(hThread3);
    printf("Done.\n");
    return 0;
}
