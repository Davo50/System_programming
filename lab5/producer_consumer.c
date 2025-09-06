/* producer_consumer.c
   Compile: cl /nologo /W3 /Ox producer_consumer.c
*/
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int value;
    struct Node* next;
} Node;

static Node* head = NULL;
static CRITICAL_SECTION cs;
static HANDLE hItems;  // counts items in list
static HANDLE hSpace;  // counts available space (optional)
static volatile LONG total_produced = 0;
static volatile LONG total_consumed = 0;
static int PRODUCE_EACH = 10;
static int MAX_QUEUE = 100;

void push_item(int v) {
    Node* n = (Node*)malloc(sizeof(Node));
    n->value = v;
    EnterCriticalSection(&cs);
    n->next = head;
    head = n;
    LeaveCriticalSection(&cs);
}

int pop_item(int* out) {
    EnterCriticalSection(&cs);
    if (!head) {
        LeaveCriticalSection(&cs);
        return 0;
    }
    Node* n = head;
    head = head->next;
    *out = n->value;
    free(n);
    LeaveCriticalSection(&cs);
    return 1;
}

DWORD WINAPI Producer(LPVOID arg) {
    int id = (int)(intptr_t)arg;
    for (int i = 0; i < PRODUCE_EACH; ++i) {
        WaitForSingleObject(hSpace, INFINITE); // wait for space
        int val = id * 1000 + i;
        push_item(val);
        InterlockedIncrement(&total_produced);
        printf("[Producer %d] produced %d (total produced: %ld)\n", id, val, total_produced);
        ReleaseSemaphore(hItems, 1, NULL);
        Sleep((rand() % 200) + 50);
    }
    return 0;
}

DWORD WINAPI Consumer(LPVOID arg) {
    int id = (int)(intptr_t)arg;
    while (1) {
        WaitForSingleObject(hItems, INFINITE);
        int val;
        if (pop_item(&val)) {
            InterlockedIncrement(&total_consumed);
            printf("  [Consumer %d] consumed %d (total consumed: %ld)\n", id, val, total_consumed);
            ReleaseSemaphore(hSpace, 1, NULL);
        }
        // stop condition: when consumed equals expected produced
        // But we don't know expected until producers finish. Use a check:
        if (InterlockedCompareExchange(&total_consumed, 0, 0) >= InterlockedCompareExchange(&total_produced, 0, 0)
            && InterlockedCompareExchange(&total_produced, 0, 0) != 0) {
            // If produced >0 and consumed >= produced -> finish
            break;
        }
        Sleep((rand() % 200) + 50);
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: %s <num_producers> <num_consumers> [items_per_producer]\n", argv[0]);
        return 1;
    }
    int np = atoi(argv[1]);
    int nc = atoi(argv[2]);
    if (argc >= 4) PRODUCE_EACH = atoi(argv[3]);
    srand((unsigned)GetTickCount());
    InitializeCriticalSection(&cs);
    MAX_QUEUE = 1000;
    hItems = CreateSemaphore(NULL, 0, MAX_QUEUE, NULL);
    hSpace = CreateSemaphore(NULL, MAX_QUEUE, MAX_QUEUE, NULL);

    HANDLE* ph = (HANDLE*)malloc(sizeof(HANDLE) * (np + nc));
    for (int i = 0; i < np; ++i) {
        ph[i] = CreateThread(NULL, 0, Producer, (LPVOID)(intptr_t)(i+1), 0, NULL);
    }
    for (int i = 0; i < nc; ++i) {
        ph[np + i] = CreateThread(NULL, 0, Consumer, (LPVOID)(intptr_t)(i+1), 0, NULL);
    }
    // wait producers
    WaitForMultipleObjects(np, ph, TRUE, INFINITE);
    // now producers finished; total_produced set
    // release consumers if they are waiting and there are no more items
    // Wait for consumers: they will exit when consumed >= produced
    WaitForMultipleObjects(nc, ph + np, TRUE, INFINITE);

    // cleanup
    for (int i = 0; i < np + nc; ++i) CloseHandle(ph[i]);
    free(ph);
    CloseHandle(hItems);
    CloseHandle(hSpace);
    DeleteCriticalSection(&cs);
    // free any remaining nodes
    while (head) {
        Node* n = head;
        head = head->next;
        free(n);
    }
    printf("All done. Produced: %ld, Consumed: %ld\n", total_produced, total_consumed);
    return 0;
}
