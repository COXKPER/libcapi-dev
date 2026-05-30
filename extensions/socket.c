#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "libcapi.h"

int socket_handler(const char *line, CapiResult *res) {
    if (strncmp(line, "socket.get ", 11) == 0) {
        char host[128] = {0};
        const char *p = line + 11;
        while (*p == ' ') p++;
        if (*p == '"') {
            p++;
            int i = 0;
            while (*p && *p != '"' && i < 127) {
                host[i++] = *p++;
            }
            host[i] = '\0';
        } else {
            strncat(res->output, "[error] socket.get: invalid syntax, expected \"host\"\n", sizeof(res->output) - strlen(res->output) - 1);
            return 1;
        }

        struct hostent *he = gethostbyname(host);
        if (!he) {
            strncat(res->output, "[error] socket.get: unknown host\n", sizeof(res->output) - strlen(res->output) - 1);
            return 1;
        }

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            strncat(res->output, "[error] socket.get: could not create socket\n", sizeof(res->output) - strlen(res->output) - 1);
            return 1;
        }

        struct sockaddr_in server;
        memset(&server, 0, sizeof(server));
        server.sin_family = AF_INET;
        server.sin_port = htons(80);
        server.sin_addr = *((struct in_addr *)he->h_addr);

        if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
            strncat(res->output, "[error] socket.get: connection failed\n", sizeof(res->output) - strlen(res->output) - 1);
            close(sock);
            return 1;
        }

        const char *req = "GET / HTTP/1.0\r\n\r\n";
        if (send(sock, req, strlen(req), 0) < 0) {
            strncat(res->output, "[error] socket.get: send failed\n", sizeof(res->output) - strlen(res->output) - 1);
            close(sock);
            return 1;
        }

        char buffer[1024];
        int received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (received > 0) {
            buffer[received] = '\0';
            capi_set_variable("SOCKET_RESPONSE", buffer);
            strncat(res->output, "Socket connected. Data received and stored in SOCKET_RESPONSE.\n", sizeof(res->output) - strlen(res->output) - 1);
        } else {
            strncat(res->output, "Socket connected but no data received.\n", sizeof(res->output) - strlen(res->output) - 1);
        }

        close(sock);
        return 1;
    }
    return 0;
}

void capi_init(void) {
    capi_register_command("socket.", socket_handler);
}
