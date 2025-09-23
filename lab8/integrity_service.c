#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <strsafe.h>

#define SERVICE_NAME L"IntegrityWatcherSvc"
#define DEFAULT_LIST L"integrity_list.txt"
#define HALF_HOUR_SECONDS (30*60)

SERVICE_STATUS_HANDLE gSvcStatusHandle = NULL;
HANDLE gStopEvent = NULL;
wchar_t g_listpath[MAX_PATH] = DEFAULT_LIST;

void WriteEvent(WORD type, const wchar_t *msg) {
    HANDLE h = RegisterEventSourceW(NULL, SERVICE_NAME);
    if (!h) return;
    ReportEventW(h, type, 0, 0x1000, NULL, 1, 0, (LPCWSTR*)&msg, NULL);
    DeregisterEventSource(h);
}

typedef struct {
    wchar_t regspec[512];
} REGWATCH_PARAM;

HKEY parse_root_key_for_regwatch(const wchar_t *full, const wchar_t **subpath_ptr) {
    if (_wcsnicmp(full, L"HKEY_LOCAL_MACHINE\\", 19) == 0) { *subpath_ptr = full + 19; return HKEY_LOCAL_MACHINE; }
    if (_wcsnicmp(full, L"HKLM\\", 5) == 0) { *subpath_ptr = full + 5; return HKEY_LOCAL_MACHINE; }
    if (_wcsnicmp(full, L"HKEY_CURRENT_USER\\", 19) == 0) { *subpath_ptr = full + 19; return HKEY_CURRENT_USER; }
    if (_wcsnicmp(full, L"HKCU\\", 5) == 0) { *subpath_ptr = full + 5; return HKEY_CURRENT_USER; }
    if (_wcsnicmp(full, L"HKEY_CLASSES_ROOT\\", 18) == 0) { *subpath_ptr = full + 18; return HKEY_CLASSES_ROOT; }
    if (_wcsnicmp(full, L"HKCR\\", 5) == 0) { *subpath_ptr = full + 5; return HKEY_CLASSES_ROOT; }
    if (_wcsnicmp(full, L"HKEY_USERS\\", 11) == 0) { *subpath_ptr = full + 11; return HKEY_USERS; }
    if (_wcsnicmp(full, L"HKU\\", 4) == 0) { *subpath_ptr = full + 4; return HKEY_USERS; }
    if (_wcsnicmp(full, L"HKEY_CURRENT_CONFIG\\", 20) == 0) { *subpath_ptr = full + 20; return HKEY_CURRENT_CONFIG; }
    if (_wcsnicmp(full, L"HKCC\\", 5) == 0) { *subpath_ptr = full + 5; return HKEY_CURRENT_CONFIG; }
    *subpath_ptr = full;
    return NULL;
}

DWORD WINAPI RegWatchThread(LPVOID param) {
    if (!param) return 1;
    REGWATCH_PARAM *p = (REGWATCH_PARAM*)param;
    const wchar_t *subpath = NULL;
    HKEY root = parse_root_key_for_regwatch(p->regspec, &subpath);
    if (!root) {
        wchar_t msg[512]; StringCchPrintfW(msg, _countof(msg), L"RegWatch: unknown root for %s", p->regspec);
        WriteEvent(EVENTLOG_ERROR_TYPE, msg);
        free(param);
        return 2;
    }

    HKEY hKey = NULL;
    LONG rc = RegOpenKeyExW(root, subpath, 0, KEY_NOTIFY, &hKey);
    if (rc != ERROR_SUCCESS) {
        wchar_t msg[512]; StringCchPrintfW(msg, _countof(msg), L"RegWatch: RegOpenKeyEx failed for %s (err=%u)", p->regspec, rc);
        WriteEvent(EVENTLOG_ERROR_TYPE, msg);
        free(param);
        return 3;
    }

    wchar_t startedMsg[512]; StringCchPrintfW(startedMsg, _countof(startedMsg), L"RegWatch started for %s", p->regspec);
    WriteEvent(EVENTLOG_INFORMATION_TYPE, startedMsg);

    // Watch loop
    while (WaitForSingleObject(gStopEvent, 0) == WAIT_TIMEOUT) {
        // synchronous notify call (will return when change occurs)
        rc = RegNotifyChangeKeyValue(hKey, TRUE,
            REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_ATTRIBUTES | REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_CHANGE_SECURITY,
            NULL, FALSE);
        if (rc == ERROR_SUCCESS) {
            wchar_t evmsg[512]; StringCchPrintfW(evmsg, _countof(evmsg), L"Registry key changed: %s", p->regspec);
            WriteEvent(EVENTLOG_INFORMATION_TYPE, evmsg);
            // After notification, we can trigger a full verification by signaling main loop via EventLog message.
            // Sleep briefly to avoid busy loop on flapping keys
            Sleep(500);
        } else {
            wchar_t evmsg[512]; StringCchPrintfW(evmsg, _countof(evmsg), L"RegNotifyChangeKeyValue failed for %s (err=%u)", p->regspec, rc);
            WriteEvent(EVENTLOG_ERROR_TYPE, evmsg);
            // wait a bit before retrying
            Sleep(1000);
        }
    }

    RegCloseKey(hKey);
    free(param);
    wchar_t stopMsg[256]; StringCchPrintfW(stopMsg, _countof(stopMsg), L"RegWatch stopped for %s", p->regspec);
    WriteEvent(EVENTLOG_INFORMATION_TYPE, stopMsg);
    return 0;
}

/* ----- File/dir change watches ----- */
typedef struct {
    HANDLE hChange;
    wchar_t parent[MAX_PATH];
} WatchHandle;

WatchHandle *gWatches = NULL;
size_t gWatchesCount = 0;

void cleanup_watches() {
    if (gWatches) {
        for (size_t i = 0; i < gWatchesCount; ++i) {
            if (gWatches[i].hChange && gWatches[i].hChange != INVALID_HANDLE_VALUE)
                FindCloseChangeNotification(gWatches[i].hChange);
        }
        free(gWatches);
        gWatches = NULL;
        gWatchesCount = 0;
    }
}

void perform_full_check_and_report() {
    wchar_t cmdline[1024];
    StringCchPrintfW(cmdline, _countof(cmdline), L"integrity_tool.exe verify \"%s\"", g_listpath);
    STARTUPINFOW si = {0}; PROCESS_INFORMATION pi = {0}; si.cb = sizeof(si);
    if (CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        // Wait up to 60 seconds
        DWORD wait = WaitForSingleObject(pi.hProcess, 1000 * 60);
        DWORD code = 0;
        if (GetExitCodeProcess(pi.hProcess, &code)) {
            wchar_t msg[256]; StringCchPrintfW(msg, _countof(msg), L"Verification finished, exit code %u", code);
            WriteEvent(EVENTLOG_INFORMATION_TYPE, msg);
        } else {
            WriteEvent(EVENTLOG_ERROR_TYPE, L"Verification finished but couldn't get exit code");
        }
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    } else {
        wchar_t msg[256]; StringCchPrintfW(msg, _countof(msg), L"Failed to launch integrity_tool.exe (err=%u)", GetLastError());
        WriteEvent(EVENTLOG_ERROR_TYPE, msg);
    }
}

void load_list_and_setup_watches() {
    FILE *f = _wfopen(g_listpath, L"rt, ccs=UTF-8");
    if (!f) {
        wchar_t msg[512]; StringCchPrintfW(msg, _countof(msg), L"Could not open list file: %s", g_listpath);
        WriteEvent(EVENTLOG_ERROR_TYPE, msg);
        return;
    }

    gWatches = (WatchHandle*)calloc(256, sizeof(WatchHandle));
    gWatchesCount = 0;

    wchar_t line[4096];
    while (fgetws(line, _countof(line), f)) {
        wchar_t *nl = wcschr(line, L'\n'); if (nl) *nl = 0;
        wchar_t *sep1 = wcschr(line, L'|'); if (!sep1) continue;
        *sep1 = 0; wchar_t *type = line; wchar_t *rest = sep1 + 1;
        wchar_t *sep2 = wcschr(rest, L'|'); if (!sep2) continue;
        *sep2 = 0; wchar_t *path = rest; wchar_t *md5 = sep2 + 1;

        if (_wcsicmp(type, L"FILE") == 0 || _wcsicmp(type, L"DIR") == 0) {
            wchar_t parent[MAX_PATH];
            StringCchCopyW(parent, MAX_PATH, path);
            wchar_t *last = wcsrchr(parent, L'\\');
            if (last && last != parent) {
                *last = 0;
            } else {
                StringCchCopyW(parent, MAX_PATH, L".");
            }
            HANDLE h = FindFirstChangeNotificationW(parent, TRUE,
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_ATTRIBUTES);
            if (h != INVALID_HANDLE_VALUE && gWatchesCount < 256) {
                gWatches[gWatchesCount].hChange = h;
                StringCchCopyW(gWatches[gWatchesCount].parent, MAX_PATH, parent);
                gWatchesCount++;
                wchar_t msg[512]; StringCchPrintfW(msg, _countof(msg), L"Watch installed for parent=%s (target=%s)", parent, path);
                WriteEvent(EVENTLOG_INFORMATION_TYPE, msg);
            } else {
                wchar_t msg[512]; StringCchPrintfW(msg, _countof(msg), L"Failed to install watch for %s (parent=%s)", path, parent);
                WriteEvent(EVENTLOG_ERROR_TYPE, msg);
            }
        } else if (_wcsicmp(type, L"REG") == 0) {
            REGWATCH_PARAM *param = (REGWATCH_PARAM*)malloc(sizeof(REGWATCH_PARAM));
            StringCchCopyW(param->regspec, _countof(param->regspec), path);
            HANDLE th = CreateThread(NULL, 0, RegWatchThread, param, 0, NULL);
            if (th) {
                CloseHandle(th);
            } else {
                wchar_t msg[512]; StringCchPrintfW(msg, _countof(msg), L"Failed to create reg watch for %s (err=%u)", path, GetLastError());
                WriteEvent(EVENTLOG_ERROR_TYPE, msg);
                free(param);
            }
        } else {
            wchar_t msg[256]; StringCchPrintfW(msg, _countof(msg), L"Unknown type in list: %s", type);
            WriteEvent(EVENTLOG_WARNING_TYPE, msg);
        }
    }

    fclose(f);
    wchar_t finalmsg[256]; StringCchPrintfW(finalmsg, _countof(finalmsg), L"Installed %zu file/dir watches", gWatchesCount);
    WriteEvent(EVENTLOG_INFORMATION_TYPE, finalmsg);
}

void WINAPI ServiceCtrlHandler(DWORD ctrl) {
    switch (ctrl) {
    case SERVICE_CONTROL_STOP:
        if (gStopEvent) SetEvent(gStopEvent);
        WriteEvent(EVENTLOG_INFORMATION_TYPE, L"Stop control received");
        break;
    default:
        break;
    }
}

void WINAPI ServiceMain(DWORD argc, LPWSTR *argv) {
    SERVICE_STATUS svcStatus = {0};
    svcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    svcStatus.dwCurrentState = SERVICE_START_PENDING;
    svcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    gSvcStatusHandle = RegisterServiceCtrlHandlerW(SERVICE_NAME, ServiceCtrlHandler);
    if (!gSvcStatusHandle) return;

    svcStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(gSvcStatusHandle, &svcStatus);

    gStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!gStopEvent) {
        WriteEvent(EVENTLOG_ERROR_TYPE, L"Failed to create stop event");
        return;
    }

    WriteEvent(EVENTLOG_INFORMATION_TYPE, L"IntegrityWatcherSvc started");
    load_list_and_setup_watches();

    DWORD timeout = HALF_HOUR_SECONDS * 1000;
    while (WaitForSingleObject(gStopEvent, timeout) == WAIT_TIMEOUT) {
        WriteEvent(EVENTLOG_INFORMATION_TYPE, L"Periodic integrity check start");
        perform_full_check_and_report();
        if (gWatches) {
            for (size_t i = 0; i < gWatchesCount; ++i) {
            }
        }
    }

    cleanup_watches();
    WriteEvent(EVENTLOG_INFORMATION_TYPE, L"IntegrityWatcherSvc stopping");
    CloseHandle(gStopEvent); gStopEvent = NULL;

    svcStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(gSvcStatusHandle, &svcStatus);
}

int wmain(int argc, wchar_t **argv) {
    if (argc >= 2) {
        if (_wcsicmp(argv[1], L"-install") == 0 || _wcsicmp(argv[1], L"install") == 0) {
            wchar_t exePath[MAX_PATH]; GetModuleFileNameW(NULL, exePath, MAX_PATH);
            if (argc >= 3) { StringCchCopyW(g_listpath, MAX_PATH, argv[2]); }
            wprintf(L"To install service (run elevated):\n");
            wprintf(L"  sc create %s binPath= \"%s --service --list \\\"%s\\\"\" start= auto\n", SERVICE_NAME, exePath, g_listpath);
            wprintf(L"Then start: sc start %s\n", SERVICE_NAME);
            return 0;
        } else if (_wcsicmp(argv[1], L"-remove") == 0 || _wcsicmp(argv[1], L"remove") == 0) {
            wprintf(L"To remove service (run elevated):\n  sc stop %s\n  sc delete %s\n", SERVICE_NAME, SERVICE_NAME);
            return 0;
        } else if (_wcsicmp(argv[1], L"--service") == 0) {
            for (int i = 2; i < argc; ++i) {
                if (_wcsicmp(argv[i], L"--list") == 0 && i + 1 < argc) { StringCchCopyW(g_listpath, MAX_PATH, argv[++i]); }
            }
            SERVICE_TABLE_ENTRYW st[] = { { (LPWSTR)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTIONW)ServiceMain }, { NULL, NULL } };
            StartServiceCtrlDispatcherW(st);
            return 0;
        }
    }

    wprintf(L"Integrity service helper\n");
    wprintf(L"Usage:\n  integrity_service -install [listfile]\n  integrity_service -remove\n  integrity_service --service --list <listfile>\n");
    wprintf(L"Note: to install the service run the printed sc create command from an elevated prompt.\n");
    return 0;
}
