#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <dbt.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <tchar.h>
#include <stdio.h>
#include <strsafe.h>
#include <time.h>
#include <stdarg.h>    // <- добавлено

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

// Если на системе GUID_DEVINTERFACE_USB_DEVICE не определён, объявим его здесь:
#ifndef GUID_DEVINTERFACE_USB_DEVICE
// {A5DCBF10-6530-11D2-901F-00C04FB951ED}
static const GUID GUID_DEVINTERFACE_USB_DEVICE =
    {0xA5DCBF10, 0x6530, 0x11D2, {0x90,0x1F,0x00,0xC0,0x4F,0xB9,0x51,0xED}};
#endif

// Registry configuration path (HKLM)
#define SERVICE_REG_PATH L"SOFTWARE\\UsbMonitorService"
#define REG_VALUE_FILEPATH L"LogFilePath"            // REG_SZ
#define REG_VALUE_BLOCKED_SERIALS L"BlockedSerials"  // REG_MULTI_SZ

#define SERVICE_NAME L"UsbMonitorService"
#define EVENT_SOURCE_NAME L"UsbMonitorService"

// Globals
SERVICE_STATUS_HANDLE g_ServiceStatusHandle = NULL;
HANDLE g_hStopEvent = NULL;
HDEVNOTIFY g_hDevNotify = NULL;
HANDLE g_hEventSource = NULL;

// Forward declarations
void WriteEventLog(WORD wType, LPCWSTR format, ...);
BOOL ReadRegistryStrings(LPCWSTR valueName, LPWSTR *outBuf, DWORD *outSizeBytes);
BOOL ReadRegistryString(LPCWSTR valueName, LPWSTR *outStr);
void AppendLogToFile(const WCHAR *filepath, const WCHAR *msg);
void OnDeviceArrivedOrRemoved(WPARAM wParam, LPARAM lParam, BOOL arrived);
void TryBlockDeviceByInstanceId(LPCWSTR deviceInstanceId);
void WINAPI ServiceMain(DWORD dwArgc, LPWSTR *lpszArgv);
DWORD WINAPI ServiceCtrlHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);
BOOL RegisterForDeviceNotifications(SERVICE_STATUS_HANDLE hService);
void Cleanup();

// Utilities
static void GetTimestampW(wchar_t *buf, size_t buflen) {
    time_t t = time(NULL);
    struct tm tmv;
    localtime_s(&tmv, &t);
    wcsftime(buf, buflen, L"%Y-%m-%d %H:%M:%S", &tmv);
}

void WriteEventLog(WORD wType, LPCWSTR format, ...) {
    WCHAR buffer[1024];
    va_list args;
    va_start(args, format);
    vswprintf_s(buffer, _countof(buffer), format, args);
    va_end(args);

    if (!g_hEventSource) {
        g_hEventSource = RegisterEventSourceW(NULL, EVENT_SOURCE_NAME);
    }
    if (g_hEventSource) {
        LPCWSTR msgs[1] = { buffer };
        ReportEventW(g_hEventSource, wType, 0, 0, NULL, 1, 0, msgs, NULL);
    }
}

// Read REG_SZ
BOOL ReadRegistryString(LPCWSTR valueName, LPWSTR *outStr) {
    HKEY hKey = NULL;
    LONG rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, SERVICE_REG_PATH, 0, KEY_READ, &hKey);
    if (rc != ERROR_SUCCESS) return FALSE;

    DWORD type = 0, size = 0;
    rc = RegQueryValueExW(hKey, valueName, NULL, &type, NULL, &size);
    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        RegCloseKey(hKey);
        return FALSE;
    }

    LPWSTR buf = (LPWSTR)malloc(size);
    if (!buf) { RegCloseKey(hKey); return FALSE; }
    rc = RegQueryValueExW(hKey, valueName, NULL, NULL, (LPBYTE)buf, &size);
    RegCloseKey(hKey);
    if (rc != ERROR_SUCCESS) { free(buf); return FALSE; }
    *outStr = buf;
    return TRUE;
}

// Read REG_MULTI_SZ into allocated buffer (caller must free)
BOOL ReadRegistryStrings(LPCWSTR valueName, LPWSTR *outBuf, DWORD *outSizeBytes) {
    HKEY hKey = NULL;
    LONG rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, SERVICE_REG_PATH, 0, KEY_READ, &hKey);
    if (rc != ERROR_SUCCESS) return FALSE;

    DWORD type = 0, size = 0;
    rc = RegQueryValueExW(hKey, valueName, NULL, &type, NULL, &size);
    if (rc != ERROR_SUCCESS || type != REG_MULTI_SZ) {
        RegCloseKey(hKey);
        return FALSE;
    }

    LPWSTR buf = (LPWSTR)malloc(size);
    if (!buf) { RegCloseKey(hKey); return FALSE; }
    rc = RegQueryValueExW(hKey, valueName, NULL, NULL, (LPBYTE)buf, &size);
    RegCloseKey(hKey);
    if (rc != ERROR_SUCCESS) { free(buf); return FALSE; }
    *outBuf = buf;
    if (outSizeBytes) *outSizeBytes = size;
    return TRUE;
}

void AppendLogToFile(const WCHAR *filepath, const WCHAR *msg) {
    if (!filepath || !msg) return;
    HANDLE h = CreateFileW(filepath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        WriteEventLog(EVENTLOG_ERROR_TYPE, L"Failed to open log file '%s' (err %u)", filepath, GetLastError());
        return;
    }
    DWORD written = 0;
    // convert msg (WCHAR) to UTF-8 for file
    int utf8len = WideCharToMultiByte(CP_UTF8, 0, msg, -1, NULL, 0, NULL, NULL);
    if (utf8len > 0) {
        char *utf8 = (char*)malloc(utf8len);
        if (utf8) {
            WideCharToMultiByte(CP_UTF8, 0, msg, -1, utf8, utf8len, NULL, NULL);
            WriteFile(h, utf8, (DWORD)(strlen(utf8)), &written, NULL);
            // add newline
            const char nl = '\n';
            WriteFile(h, &nl, 1, &written, NULL);
            free(utf8);
        }
    }
    CloseHandle(h);
}

// Called when device arrives/removes
void OnDeviceArrivedOrRemoved(WPARAM wParam, LPARAM lParam, BOOL arrived) {
    (void)wParam; // убрать предупреждение о неиспользуемом параметре
    PDEV_BROADCAST_HDR pHdr = (PDEV_BROADCAST_HDR)lParam;
    if (!pHdr) return;

    if (pHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE || pHdr->dbch_devicetype == DBT_DEVTYP_HANDLE) {
        PDEV_BROADCAST_DEVICEINTERFACE_W pDi = (PDEV_BROADCAST_DEVICEINTERFACE_W)pHdr;
        LPCWSTR devName = pDi->dbcc_name; // might be like \\?\USB#VID_xxxx&PID_xxxx#SERIAL#{...}
        wchar_t deviceInstanceId[1024] = {0};

        // try to extract device instance id from name; some cases already contain instance id after last backslash
        if (devName && wcslen(devName) < _countof(deviceInstanceId)) {
            // strip leading "\\?\" if present
            LPCWSTR p = devName;
            if (wcsncmp(p, L"\\\\?\\", 4) == 0) p += 4;
            // replace '#' with '\' to form instance? Often device interface name contains '#' segments.
            size_t j=0;
            for (size_t i=0; p[i] && j < _countof(deviceInstanceId)-1; ++i) {
                WCHAR ch = p[i];
                deviceInstanceId[j++] = (ch == L'#') ? L'\\' : ch;
            }
            deviceInstanceId[j]=0;
        }

        // Extract serial from instance id (last segment after last backslash)
        WCHAR serial[256] = L"(unknown)";
        LPCWSTR lastSlash = wcsrchr(deviceInstanceId, L'\\');
        if (lastSlash && *(lastSlash+1)) {
            wcsncpy_s(serial, _countof(serial), lastSlash+1, _TRUNCATE);
        } else if (devName) {
            LPCWSTR lastHash = wcsrchr(devName, L'#');
            if (lastHash && *(lastHash+1)) {
                size_t i=0;
                for (LPCWSTR q = lastHash+1; *q && *q != L'#' && *q != L'{' && i < _countof(serial)-1; ++q, ++i) serial[i]=*q;
                serial[i]=0;
            }
        }

        wchar_t ts[64]; GetTimestampW(ts, _countof(ts));
        WCHAR evmsg[1024];
        StringCchPrintfW(evmsg, _countof(evmsg), L"%s - USB %s: instance='%s' serial='%s'",
                         ts, arrived ? L"ARRIVED" : L"REMOVED", deviceInstanceId[0] ? deviceInstanceId : L"(none)", serial);

        // Log to Event Viewer
        WriteEventLog(EVENTLOG_INFORMATION_TYPE, evmsg);

        // Append to file (path from registry)
        LPWSTR logfile = NULL;
        if (ReadRegistryString(REG_VALUE_FILEPATH, &logfile)) {
            AppendLogToFile(logfile, evmsg);
            free(logfile);
        } else {
            // no file path configured
            WriteEventLog(EVENTLOG_WARNING_TYPE, L"No log file path set in registry at HKLM\\%s\\%s", SERVICE_REG_PATH, REG_VALUE_FILEPATH);
        }

        // If arrived, check blocked list
        if (arrived) {
            LPWSTR multi = NULL;
            DWORD sizeBytes = 0;
            if (ReadRegistryStrings(REG_VALUE_BLOCKED_SERIALS, &multi, &sizeBytes)) {
                LPWSTR cur = multi;
                BOOL blocked = FALSE;
                while (*cur) {
                    if (_wcsicmp(cur, serial) == 0) { blocked = TRUE; break; }
                    cur += wcslen(cur) + 1;
                }
                if (blocked) {
                    WriteEventLog(EVENTLOG_WARNING_TYPE, L"Device with serial '%s' is blocked by configuration; attempting to disable.", serial);
                    TryBlockDeviceByInstanceId(deviceInstanceId);
                }
                free(multi);
            }
        }
    }
}

// Attempt to disable device by instance id (best-effort)
void TryBlockDeviceByInstanceId(LPCWSTR deviceInstanceId) {
    if (!deviceInstanceId || !*deviceInstanceId) {
        WriteEventLog(EVENTLOG_ERROR_TYPE, L"TryBlockDevice: no device instance id available.");
        return;
    }

    DEVINST devinst = 0;
    CONFIGRET cr = CM_Locate_DevNodeW(&devinst, (DEVINSTID_W)deviceInstanceId, 0);
    if (cr != CR_SUCCESS) {
        WriteEventLog(EVENTLOG_ERROR_TYPE, L"CM_Locate_DevNode failed for '%s' (CR=%u)", deviceInstanceId, cr);
        return;
    }

    cr = CM_Disable_DevNode(devinst, 0);
    if (cr == CR_SUCCESS) {
        WriteEventLog(EVENTLOG_INFORMATION_TYPE, L"Device '%s' disabled (CM_Disable_DevNode succeeded).", deviceInstanceId);
    } else {
        WriteEventLog(EVENTLOG_ERROR_TYPE, L"Failed to disable device '%s' (CM_Disable_DevNode CR=%u). Attempting eject.", deviceInstanceId, cr);
        PNP_VETO_TYPE vetoType;
        WCHAR vetoName[512];
        cr = CM_Request_Device_EjectW(devinst, &vetoType, vetoName, sizeof(vetoName)/sizeof(WCHAR), 0);
        if (cr == CR_SUCCESS) {
            WriteEventLog(EVENTLOG_INFORMATION_TYPE, L"Device '%s' ejected successfully.", deviceInstanceId);
        } else {
            WriteEventLog(EVENTLOG_ERROR_TYPE, L"CM_Request_Device_EjectW failed for '%s' (CR=%u).", deviceInstanceId, cr);
        }
    }
}

// Service control handler (extended)
DWORD WINAPI ServiceCtrlHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext) {
    (void)lpContext; // убрать предупреждение
    switch (dwControl) {
    case SERVICE_CONTROL_STOP:
        if (g_ServiceStatusHandle) {
            SERVICE_STATUS ss = {0};
            ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
            ss.dwCurrentState = SERVICE_STOP_PENDING;
            ss.dwControlsAccepted = 0;
            ss.dwWin32ExitCode = 0;
            ss.dwServiceSpecificExitCode = 0;
            ss.dwCheckPoint = 1;
            ss.dwWaitHint = 3000;
            SetServiceStatus(g_ServiceStatusHandle, &ss);
        }
        if (g_hStopEvent) SetEvent(g_hStopEvent);
        return NO_ERROR;

    case SERVICE_CONTROL_DEVICEEVENT:
        if (dwEventType == DBT_DEVICEARRIVAL || dwEventType == DBT_DEVICEREMOVECOMPLETE) {
            OnDeviceArrivedOrRemoved((WPARAM)0, (LPARAM)lpEventData, (dwEventType == DBT_DEVICEARRIVAL));
        }
        return NO_ERROR;

    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;

    default:
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

// Register for device notifications using service handle (DEVICE_NOTIFY_SERVICE_HANDLE)
BOOL RegisterForDeviceNotifications(SERVICE_STATUS_HANDLE hService) {
    DEV_BROADCAST_DEVICEINTERFACE_W NotificationFilter;
    ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
    NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE_W);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    NotificationFilter.dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE;

    HDEVNOTIFY h = RegisterDeviceNotificationW((HANDLE)hService, &NotificationFilter, DEVICE_NOTIFY_SERVICE_HANDLE);
    if (!h) {
        WriteEventLog(EVENTLOG_ERROR_TYPE, L"RegisterDeviceNotification failed (err %u)", GetLastError());
        return FALSE;
    }
    g_hDevNotify = h;
    WriteEventLog(EVENTLOG_INFORMATION_TYPE, L"Device notifications registered.");
    return TRUE;
}

void Cleanup() {
    if (g_hDevNotify) {
        UnregisterDeviceNotification(g_hDevNotify);
        g_hDevNotify = NULL;
    }
    if (g_hEventSource) {
        DeregisterEventSource(g_hEventSource);
        g_hEventSource = NULL;
    }
    if (g_hStopEvent) {
        CloseHandle(g_hStopEvent);
        g_hStopEvent = NULL;
    }
}

void WINAPI ServiceMain(DWORD dwArgc, LPWSTR *lpszArgv) {
    SERVICE_STATUS ss = {0};
    ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    ss.dwCurrentState = SERVICE_START_PENDING;
    ss.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PARAMCHANGE | SERVICE_ACCEPT_SHUTDOWN;
    ss.dwWin32ExitCode = 0;
    ss.dwServiceSpecificExitCode = 0;
    ss.dwCheckPoint = 0;
    ss.dwWaitHint = 3000;

    g_ServiceStatusHandle = RegisterServiceCtrlHandlerExW(SERVICE_NAME, ServiceCtrlHandlerEx, NULL);
    if (!g_ServiceStatusHandle) {
        return;
    }

    g_hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

    ss.dwCurrentState = SERVICE_RUNNING;
    ss.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PARAMCHANGE | SERVICE_ACCEPT_SHUTDOWN;
    SetServiceStatus(g_ServiceStatusHandle, &ss);

    RegisterForDeviceNotifications(g_ServiceStatusHandle);

    WriteEventLog(EVENTLOG_INFORMATION_TYPE, L"%s started.", SERVICE_NAME);

    while (WaitForSingleObject(g_hStopEvent, 500) == WAIT_TIMEOUT) {
        // idle
    }

    WriteEventLog(EVENTLOG_INFORMATION_TYPE, L"%s stopping...", SERVICE_NAME);

    Cleanup();

    ss.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_ServiceStatusHandle, &ss);
}

// Service entry point
int wmain(int argc, wchar_t *argv[]) {
    SERVICE_TABLE_ENTRYW DispatchTable[] = {
        { (LPWSTR)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTIONW)ServiceMain },
        { NULL, NULL }
    };
    if (!StartServiceCtrlDispatcherW(DispatchTable)) {
        DWORD err = GetLastError();
        wprintf(L"StartServiceCtrlDispatcher failed: %u. If you want to run for debugging, start with /console.\n", err);
        if (argc > 1 && _wcsicmp(argv[1], L"/console") == 0) {
            g_hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
            g_ServiceStatusHandle = (SERVICE_STATUS_HANDLE)0x1; // fake handle for RegisterDeviceNotification
            RegisterForDeviceNotifications(g_ServiceStatusHandle);
            WriteEventLog(EVENTLOG_INFORMATION_TYPE, L"Running in console mode (debug). Press Ctrl+C to exit.");
            while (1) Sleep(1000);
        }
        return 1;
    }
    return 0;
}
