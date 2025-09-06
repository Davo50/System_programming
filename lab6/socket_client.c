#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>

#pragma comment(lib, "ws2_32.lib")

#define CHUNK 8192

DWORD WINAPI recv_thread(LPVOID arg) {
    SOCKET s = (SOCKET)(size_t)arg;
    char line[1024];
    while (1) {
        // read header line
        int idx = 0;
        char c;
        int r;
        while (idx < sizeof(line)-1) {
            r = recv(s, &c, 1, 0);
            if (r <= 0) goto done;
            line[idx++] = c;
            if (c == '\n') break;
        }
        line[idx] = 0;
        if (strncmp(line, "MSG:", 4) == 0) {
            printf("%s", line+4); // print sender+message
        } else if (strncmp(line, "FILE:", 5) == 0) {
            char filename[512];
            long long filesize = 0;
            if (sscanf(line+5, "%511[^:]:%lld", filename, &filesize) < 2) {
                printf("Bad FILE header\n");
                continue;
            }
            // open file to write
            char outname[600];
            snprintf(outname, sizeof(outname), "recv_%s", filename);
            FILE *f = fopen(outname, "wb");
            if (!f) {
                printf("Cannot create file %s\n", outname);
                // still need to drain incoming bytes
                char tmp[CHUNK];
                long long rem = filesize;
                while (rem > 0) {
                    int toread = (int)(rem > CHUNK ? CHUNK : rem);
                    r = recv(s, tmp, toread, 0);
                    if (r <= 0) goto done;
                    rem -= r;
                }
                continue;
            }
            long long rem = filesize;
            while (rem > 0) {
                int toread = (int)(rem > CHUNK ? CHUNK : rem);
                r = recv(s, line, toread, 0);
                if (r <= 0) { fclose(f); goto done; }
                fwrite(line, 1, r, f);
                rem -= r;
            }
            fclose(f);
            printf("Received file '%s' saved as '%s' (%lld bytes)\n", filename, outname, filesize);
        } else {
            // unknown -- print raw
            printf("RECV RAW: %s", line);
        }
    }
done:
    printf("Connection closed by server.\n");
    return 0;
}

int send_file(SOCKET s, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { printf("Cannot open %s\n", path); return 1; }
    // extract filename
    const char *p = strrchr(path, '\\');
    if (!p) p = strrchr(path, '/');
    const char *filename = p ? p+1 : path;
    fseek(f, 0, SEEK_END);
    long long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char header[1024];
    int hl = snprintf(header, sizeof(header), "FILE:%s:%lld\n", filename, size);
    send(s, header, hl, 0);

    char buf[CHUNK];
    while (!feof(f)) {
        int r = (int)fread(buf, 1, sizeof(buf), f);
        if (r > 0) {
            int sent = 0;
            while (sent < r) {
                int w = send(s, buf + sent, r - sent, 0);
                if (w <= 0) { fclose(f); return 1; }
                sent += w;
            }
        }
    }
    fclose(f);
    printf("File %s sent (%lld bytes)\n", filename, size);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <server_ip> <port>\n", argv[0]);
        return 1;
    }
    const char *server = argv[1];
    const char *port = argv[2];

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) { printf("WSAStartup failed\n"); return 1; }

    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(server, port, &hints, &res) != 0) { printf("getaddrinfo failed\n"); WSACleanup(); return 1; }

    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) { printf("socket failed\n"); freeaddrinfo(res); WSACleanup(); return 1; }

    if (connect(s, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        printf("connect failed\n"); closesocket(s); freeaddrinfo(res); WSACleanup(); return 1;
    }
    freeaddrinfo(res);

    printf("Connected to %s:%s\n", server, port);

    HANDLE rt = CreateThread(NULL, 0, recv_thread, (LPVOID)(size_t)s, 0, NULL);

    // read stdin
    char input[1024];
    while (1) {
        if (!fgets(input, sizeof(input), stdin)) break;
        // trim newline
        size_t L = strlen(input);
        if (L > 0 && input[L-1] == '\n') input[L-1] = 0;

        if (strncmp(input, "/send ", 6) == 0) {
            const char *path = input + 6;
            if (send_file(s, path) != 0) {
                printf("Error sending file\n");
            }
        } else if (strcmp(input, "/quit") == 0) {
            break;
        } else {
            char msg[1200];
            int hl = snprintf(msg, sizeof(msg), "MSG:%s\n", input);
            send(s, msg, hl, 0);
        }
    }

    closesocket(s);
    WaitForSingleObject(rt, 1000);
    CloseHandle(rt);
    WSACleanup();
    return 0;
}
