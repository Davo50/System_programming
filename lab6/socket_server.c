#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>

#pragma comment(lib, "ws2_32.lib")

#define BACKLOG 5
#define CHUNK 8192

typedef struct {
    SOCKET sock;
    int id;
    SOCKET *other;
} CLIENT_CTX;

int recv_line(SOCKET s, char *buf, int maxlen) {
    int total = 0;
    char c;
    int r;
    while (total < maxlen - 1) {
        r = recv(s, &c, 1, 0);
        if (r <= 0) return r;
        buf[total++] = c;
        if (c == '\n') break;
    }
    buf[total] = 0;
    return total;
}

DWORD WINAPI client_thread(LPVOID arg) {
    CLIENT_CTX *ctx = (CLIENT_CTX*)arg;
    SOCKET s = ctx->sock;
    SOCKET other = *(ctx->other);
    char line[1024];
    printf("Client %d thread started\n", ctx->id);

    while (1) {
        int r = recv_line(s, line, sizeof(line));
        if (r <= 0) break;

        if (strncmp(line, "MSG:", 4) == 0) {
            if (other != INVALID_SOCKET) send(other, line, r, 0);
        } else if (strncmp(line, "FILE:", 5) == 0) {
            char filename[512];
            long long filesize = 0;
            if (sscanf(line+5, "%511[^:]:%lld", filename, &filesize) < 2) {
                printf("Bad FILE header from client %d\n", ctx->id);
                continue;
            }
            if (other != INVALID_SOCKET) send(other, line, r, 0);

            long long remaining = filesize;
            char buffer[CHUNK];
            while (remaining > 0) {
                int toread = (int) (remaining > CHUNK ? CHUNK : remaining);
                int got = recv(s, buffer, toread, 0);
                if (got <= 0) goto cleanup;
                if (other != INVALID_SOCKET) {
                    int sent = 0;
                    while (sent < got) {
                        int w = send(other, buffer + sent, got - sent, 0);
                        if (w <= 0) goto cleanup;
                        sent += w;
                    }
                }
                remaining -= got;
            }
            printf("Client %d sent file '%s' (%lld bytes) forwarded\n", ctx->id, filename, filesize);
        } else {
            if (other != INVALID_SOCKET) send(other, line, r, 0);
        }
    }

cleanup:
    printf("Client %d disconnected\n", ctx->id);
    closesocket(s);
    ctx->sock = INVALID_SOCKET;
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }
    const char *port = argv[1];

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, port, &hints, &res) != 0) {
        printf("getaddrinfo failed\n");
        WSACleanup();
        return 1;
    }

    SOCKET listen_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (listen_sock == INVALID_SOCKET) {
        printf("socket failed\n");
        freeaddrinfo(res); WSACleanup(); return 1;
    }

    if (bind(listen_sock, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        printf("bind failed\n"); closesocket(listen_sock); freeaddrinfo(res); WSACleanup(); return 1;
    }
    freeaddrinfo(res);

    if (listen(listen_sock, BACKLOG) == SOCKET_ERROR) {
        printf("listen failed\n"); closesocket(listen_sock); WSACleanup(); return 1;
    }

    printf("Socket server listening on port %s\nWaiting for two clients...\n", port);

    SOCKET s1 = INVALID_SOCKET, s2 = INVALID_SOCKET;
    struct sockaddr_in cliaddr;
    int addrlen = sizeof(cliaddr);

    s1 = accept(listen_sock, (struct sockaddr*)&cliaddr, &addrlen);
    if (s1 == INVALID_SOCKET) { printf("accept failed\n"); closesocket(listen_sock); WSACleanup(); return 1; }
    printf("Client 1 connected\n");
    send(s1, "MSG:Server: waiting for second client...\n", 38, 0);

    s2 = accept(listen_sock, (struct sockaddr*)&cliaddr, &addrlen);
    if (s2 == INVALID_SOCKET) { printf("accept failed\n"); closesocket(s1); closesocket(listen_sock); WSACleanup(); return 1; }
    printf("Client 2 connected\n");
    send(s1, "MSG:Server: second client connected. You can chat now.\n", 52, 0);
    send(s2, "MSG:Server: connected. You can chat now.\n", 40, 0);

    SOCKET other1 = s2, other2 = s1;
    CLIENT_CTX ctx1 = { s1, 1, &other1 };
    CLIENT_CTX ctx2 = { s2, 2, &other2 };

    HANDLE t1 = CreateThread(NULL, 0, client_thread, &ctx1, 0, NULL);
    HANDLE t2 = CreateThread(NULL, 0, client_thread, &ctx2, 0, NULL);

    WaitForSingleObject(t1, INFINITE);
    WaitForSingleObject(t2, INFINITE);

    CloseHandle(t1); CloseHandle(t2);
    closesocket(listen_sock);
    WSACleanup();
    printf("Server exiting\n");
    return 0;
}
