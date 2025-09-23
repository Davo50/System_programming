#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>

#define BUF_SIZE 8192

BOOL compute_file_md5(const wchar_t *path, char outHex[33]) {
    BOOL ok = FALSE;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE buffer[BUF_SIZE];
    DWORD read;

    hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) goto cleanup;

    if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) goto cleanup;
    if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) goto cleanup;

    while (ReadFile(hFile, buffer, BUF_SIZE, &read, NULL) && read > 0) {
        if (!CryptHashData(hHash, buffer, read, 0)) goto cleanup;
    }

    BYTE md[16];
    DWORD mdlen = sizeof(md);
    if (!CryptGetHashParam(hHash, HP_HASHVAL, md, &mdlen, 0)) goto cleanup;
    for (DWORD i = 0; i < mdlen; ++i) sprintf_s(outHex + i*2, 3, "%02x", md[i]);
    outHex[32] = 0;
    ok = TRUE;

cleanup:
    if (hHash) CryptDestroyHash(hHash);
    if (hProv) CryptReleaseContext(hProv, 0);
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    return ok;
}

BOOL md5_agg_init(HCRYPTPROV *phProv, HCRYPTHASH *phHash) {
    *phProv = 0; *phHash = 0;
    if (!CryptAcquireContextW(phProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) return FALSE;
    if (!CryptCreateHash(*phProv, CALG_MD5, 0, 0, phHash)) { CryptReleaseContext(*phProv,0); *phProv=0; return FALSE; }
    return TRUE;
}
void md5_agg_destroy(HCRYPTPROV hProv, HCRYPTHASH hHash, char outHex[33]) {
    if (hHash) {
        BYTE md[16]; DWORD mdlen = sizeof(md);
        if (CryptGetHashParam(hHash, HP_HASHVAL, md, &mdlen, 0)) {
            for (DWORD i = 0; i < mdlen; ++i) sprintf_s(outHex + i*2, 3, "%02x", md[i]);
            outHex[32] = 0;
        } else outHex[0]=0;
        CryptDestroyHash(hHash);
    }
    if (hProv) CryptReleaseContext(hProv,0);
}

typedef struct {
    wchar_t **arr; size_t cnt, cap;
} wvec;
void wvec_init(wvec *v){v->arr=NULL; v->cnt=v->cap=0;}
void wvec_push(wvec *v, const wchar_t *s){
    if (v->cnt==v->cap){ size_t n = v->cap? v->cap*2: 64; v->arr=(wchar_t**)realloc(v->arr,n*sizeof(wchar_t*)); v->cap=n;}
    v->arr[v->cnt++] = _wcsdup(s);
}
void wvec_free(wvec *v){ for(size_t i=0;i<v->cnt;i++) free(v->arr[i]); free(v->arr); v->arr=NULL; v->cnt=v->cap=0; }

void collect_files_recursive(const wchar_t *root, wvec *out) {
    WIN32_FIND_DATAW fd;
    wchar_t searchPath[MAX_PATH+8];
    StringCchCopyW(searchPath, MAX_PATH, root);
    size_t len = wcslen(searchPath);
    if (len && (searchPath[len-1] != L'\\')) StringCchCatW(searchPath, MAX_PATH, L"\\*");
    else StringCchCatW(searchPath, MAX_PATH, L"*");

    HANDLE h = FindFirstFileW(searchPath, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (wcscmp(fd.cFileName, L".")==0 || wcscmp(fd.cFileName, L"..")==0) continue;
        wchar_t path[MAX_PATH*2];
        StringCchPrintfW(path, _countof(path), L"%s\\%s", root, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            collect_files_recursive(path, out);
        } else {
            wvec_push(out, path);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

BOOL compute_dir_md5(const wchar_t *dirpath, char outHex[33]) {
    wvec files; wvec_init(&files);
    collect_files_recursive(dirpath, &files);
    if (files.cnt > 1) {
        qsort(files.arr, files.cnt, sizeof(wchar_t*), (int(*)(const void*,const void*))wcscmp);
    }
    HCRYPTPROV prov; HCRYPTHASH hash;
    if (!md5_agg_init(&prov, &hash)) { wvec_free(&files); return FALSE; }
    for (size_t i=0;i<files.cnt;i++) {
        wchar_t *p = files.arr[i];
        char fmd5[33];
        if (!compute_file_md5(p, fmd5)) {
            char temp[512];
            // include file path indicator even on error
            int need = WideCharToMultiByte(CP_UTF8,0,p,-1,NULL,0,NULL,NULL);
            char *mb = (char*)malloc(need);
            WideCharToMultiByte(CP_UTF8,0,p,-1,mb,need,NULL,NULL);
            sprintf_s(temp, sizeof(temp), "%s|<ERR>\n", mb);
            free(mb);
            CryptHashData(hash, (BYTE*)temp, (DWORD)strlen(temp), 0);
        } else {
            char tmp[1024];
            int needed = WideCharToMultiByte(CP_UTF8,0,p,-1,NULL,0,NULL,NULL);
            char *mb = (char*)malloc(needed);
            WideCharToMultiByte(CP_UTF8,0,p,-1,mb,needed,NULL,NULL);
            sprintf_s(tmp, sizeof(tmp), "%s|%s\n", mb, fmd5);
            free(mb);
            CryptHashData(hash, (BYTE*)tmp, (DWORD)strlen(tmp), 0);
        }
    }
    md5_agg_destroy(prov, hash, outHex);
    wvec_free(&files);
    return TRUE;
}

HKEY parse_root_key(const wchar_t **subpath_ptr, const wchar_t *full) {
    if (_wcsnicmp(full, L"HKEY_LOCAL_MACHINE\\", 19)==0 || _wcsnicmp(full, L"HKLM\\",5)==0) {
        *subpath_ptr = (_wcsnicmp(full, L"HKEY_LOCAL_MACHINE\\", 19)==0) ? (full+19) : (full+5);
        return HKEY_LOCAL_MACHINE;
    }
    if (_wcsnicmp(full, L"HKEY_CURRENT_USER\\",19)==0 || _wcsnicmp(full, L"HKCU\\",5)==0) {
        *subpath_ptr = (_wcsnicmp(full, L"HKEY_CURRENT_USER\\",19)==0)? (full+19):(full+5);
        return HKEY_CURRENT_USER;
    }
    if (_wcsnicmp(full, L"HKEY_CLASSES_ROOT\\",18)==0 || _wcsnicmp(full, L"HKCR\\",5)==0) {
        *subpath_ptr = (_wcsnicmp(full, L"HKEY_CLASSES_ROOT\\",18)==0)? (full+18):(full+5);
        return HKEY_CLASSES_ROOT;
    }
    if (_wcsnicmp(full, L"HKEY_USERS\\",11)==0 || _wcsnicmp(full, L"HKU\\",4)==0) {
        *subpath_ptr = (_wcsnicmp(full, L"HKEY_USERS\\",11)==0)? (full+11):(full+4);
        return HKEY_USERS;
    }
    if (_wcsnicmp(full, L"HKEY_CURRENT_CONFIG\\",20)==0 || _wcsnicmp(full, L"HKCC\\",5)==0) {
        *subpath_ptr = (_wcsnicmp(full, L"HKEY_CURRENT_CONFIG\\",20)==0)? (full+20):(full+5);
        return HKEY_CURRENT_CONFIG;
    }
    *subpath_ptr = full;
    return NULL;
}

BOOL compute_registry_md5(const wchar_t *regspec, char outHex[33]) {
    const wchar_t *subpath = NULL;
    HKEY root = parse_root_key(&subpath, regspec);
    if (!root) return FALSE;
    HKEY hKey; LONG rc = RegOpenKeyExW(root, subpath, 0, KEY_READ, &hKey);
    if (rc != ERROR_SUCCESS) return FALSE;
    HCRYPTPROV prov; HCRYPTHASH hash;
    if (!md5_agg_init(&prov, &hash)) { RegCloseKey(hKey); return FALSE; }

    DWORD idx = 0;
    wchar_t valname[512];
    BYTE data[4096];
    DWORD valname_len, data_len, type;
    while (TRUE) {
        valname_len = _countof(valname);
        data_len = sizeof(data);
        rc = RegEnumValueW(hKey, idx, valname, &valname_len, NULL, &type, data, &data_len);
        if (rc == ERROR_NO_MORE_ITEMS) break;
        if (rc == ERROR_SUCCESS) {
            int need = WideCharToMultiByte(CP_UTF8,0,valname,-1,NULL,0,NULL,NULL);
            char *namemb = (char*)malloc(need);
            WideCharToMultiByte(CP_UTF8,0,valname,-1,namemb,need,NULL,NULL);
            CryptHashData(hash, (BYTE*)namemb, (DWORD)strlen(namemb), 0);
            free(namemb);
            CryptHashData(hash, (BYTE*)&type, sizeof(type), 0);
            CryptHashData(hash, data, data_len, 0);
        }
        idx++;
    }
    md5_agg_destroy(prov, hash, outHex);
    RegCloseKey(hKey);
    return TRUE;
}

void usage() {
    printf("integrity_tool - simple integrity list utility\n");
    printf("Usage:\n");
    printf("  integrity_tool create <listfile> <obj1> [obj2 ...]   - create list (files, dirs, registry specs)\n");
    printf("  integrity_tool verify <listfile>                     - verify list and print results\n");
    printf("\n");
    printf("Object formats:\n");
    printf("  file or directory paths (absolute or relative)\n");
    printf("  registry keys with prefix REG: e.g. REG:HKEY_LOCAL_MACHINE\\Software\\MyKey\n");
    printf("\n");
}

int wmain(int argc, wchar_t **argv) {
    if (argc < 2) { usage(); return 1; }
    if (_wcsicmp(argv[1], L"create")==0) {
        if (argc < 4) { usage(); return 1; }
        const wchar_t *listfile = argv[2];
        FILE *f = NULL;
        _wfopen_s(&f, listfile, L"wt, ccs=UTF-8");
        if (!f) { wprintf(L"Cannot open output file %s\n", listfile); return 2; }
        for (int i=3;i<argc;i++) {
            const wchar_t *obj = argv[i];
            if (_wcsnicmp(obj, L"REG:", 4)==0) {
                const wchar_t *reg = obj+4;
                char md5[33] = {0};
                if (compute_registry_md5(reg, md5)) {
                    fwprintf(f, L"REG|%s|%S\n", reg, md5);
                    wprintf(L"REG  %s -> %hs\n", reg, md5);
                } else {
                    fwprintf(f, L"REG|%s|<ERR>\n", reg);
                    wprintf(L"REG  %s -> <ERR>\n", reg);
                }
            } else {
                DWORD attr = GetFileAttributesW(obj);
                if (attr == INVALID_FILE_ATTRIBUTES) {
                    fwprintf(f, L"FILE|%s|<MISSING>\n", obj);
                    wprintf(L"MISSING %s\n", obj);
                } else if (attr & FILE_ATTRIBUTE_DIRECTORY) {
                    char md5[33] = {0};
                    if (compute_dir_md5(obj, md5)) {
                        fwprintf(f, L"DIR|%s|%S\n", obj, md5);
                        wprintf(L"DIR  %s -> %hs\n", obj, md5);
                    } else {
                        fwprintf(f, L"DIR|%s|<ERR>\n", obj);
                        wprintf(L"DIR  %s -> <ERR>\n", obj);
                    }
                } else {
                    char md5[33] = {0};
                    if (compute_file_md5(obj, md5)) {
                        fwprintf(f, L"FILE|%s|%S\n", obj, md5);
                        wprintf(L"FILE %s -> %hs\n", obj, md5);
                    } else {
                        fwprintf(f, L"FILE|%s|<ERR>\n", obj);
                        wprintf(L"FILE %s -> <ERR>\n", obj);
                    }
                }
            }
        }
        fclose(f);
        wprintf(L"List saved to %s\n", listfile);
        return 0;
    } else if (_wcsicmp(argv[1], L"verify")==0) {
        if (argc < 3) { usage(); return 1; }
        const wchar_t *listfile = argv[2];
        FILE *f = NULL;
        _wfopen_s(&f, listfile, L"rt, ccs=UTF-8");
        if (!f) { wprintf(L"Cannot open list file %s\n", listfile); return 2; }
        wchar_t line[4096];
        int total=0, okcount=0, badcount=0;
        while (fgetws(line, _countof(line), f)) {
            wchar_t *nl = wcschr(line, L'\n'); if (nl) *nl=0;
            wchar_t *sep1 = wcschr(line, L'|');
            if (!sep1) continue;
            *sep1=0; wchar_t *type = line;
            wchar_t *rest = sep1+1;
            wchar_t *sep2 = wcschr(rest, L'|');
            if (!sep2) continue;
            *sep2=0; wchar_t *path = rest;
            wchar_t *md5stored = sep2+1;
            total++;
            char curmd5[33] = {0};
            BOOL ok = FALSE;
            if (_wcsicmp(type, L"FILE")==0) {
                if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) {
                    wprintf(L"[MISSING] %s\n", path); badcount++; continue;
                }
                ok = compute_file_md5(path, curmd5);
            } else if (_wcsicmp(type, L"DIR")==0) {
                if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) {
                    wprintf(L"[MISSING] %s\n", path); badcount++; continue;
                }
                ok = compute_dir_md5(path, curmd5);
            } else if (_wcsicmp(type, L"REG")==0) {
                ok = compute_registry_md5(path, curmd5);
            }
            if (!ok) {
                wprintf(L"[ERR]  %s\n", path); badcount++; continue;
            }
            if (_wcsicmp(md5stored, (wchar_t*)L"<ERR>")==0 || _wcsicmp(md5stored, (wchar_t*)L"<MISSING>")==0) {
                wprintf(L"[RECALC] %s -> %hs (was %ls)\n", path, curmd5, md5stored);
                badcount++;
            } else {
                char storedc[64]; WideCharToMultiByte(CP_UTF8,0,md5stored,-1,storedc,_countof(storedc),NULL,NULL);
                if (_stricmp(storedc, curmd5)==0) {
                    wprintf(L"[OK]   %s\n", path);
                    okcount++;
                } else {
                    wprintf(L"[FAIL] %s  expected=%hs current=%hs\n", path, storedc, curmd5);
                    badcount++;
                }
            }
        }
        fclose(f);
        wprintf(L"Total: %d  OK: %d  Problems: %d\n", total, okcount, badcount);
        return (badcount==0)?0:3;
    } else {
        usage();
        return 1;
    }
}
