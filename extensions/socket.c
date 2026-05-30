#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "libcapi.h"

static int current_client_sock = -1;

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

    if (strncmp(line, "socket.listen ", 14) == 0) {
        int port = atoi(line + 14);
        if (port <= 0) {
            strncat(res->output, "[error] socket.listen: invalid port\n", sizeof(res->output) - strlen(res->output) - 1);
            return 1;
        }

        int server_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (server_sock < 0) {
            strncat(res->output, "[error] socket.listen: could not create socket\n", sizeof(res->output) - strlen(res->output) - 1);
            return 1;
        }

        int opt = 1;
        setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in server;
        memset(&server, 0, sizeof(server));
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = INADDR_ANY;
        server.sin_port = htons(port);

        if (bind(server_sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
            close(server_sock);
            strncat(res->output, "[error] socket.listen: bind failed\n", sizeof(res->output) - strlen(res->output) - 1);
            return 1;
        }

        if (listen(server_sock, 1) < 0) {
            close(server_sock);
            strncat(res->output, "[error] socket.listen: listen failed\n", sizeof(res->output) - strlen(res->output) - 1);
            return 1;
        }

        struct sockaddr_in client;
        socklen_t c_len = sizeof(client);
        int client_sock = accept(server_sock, (struct sockaddr *)&client, &c_len);
        
        close(server_sock); // Close server so port can be reused easily in scripts

        if (client_sock < 0) {
            strncat(res->output, "[error] socket.listen: accept failed\n", sizeof(res->output) - strlen(res->output) - 1);
            return 1;
        }

        char buffer[2048] = {0};
        int received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (received > 0) {
            buffer[received] = '\0';
            capi_set_variable("SOCKET_REQUEST", buffer);
        } else {
            capi_set_variable("SOCKET_REQUEST", "");
        }

        current_client_sock = client_sock;
        strncat(res->output, "Client connected. Request data stored in SOCKET_REQUEST.\n", sizeof(res->output) - strlen(res->output) - 1);
        return 1;
    }

    if (strncmp(line, "socket.reply ", 13) == 0) {
        if (current_client_sock < 0) {
            strncat(res->output, "[error] socket.reply: no client connected\n", sizeof(res->output) - strlen(res->output) - 1);
            return 1;
        }

        char reply[2048] = {0};
        const char *p = line + 13;
        while (*p == ' ') p++;
        
        if (*p == '"') {
            p++;
            int i = 0;
            while (*p && *p != '"' && i < 2047) {
                reply[i++] = *p++;
            }
            reply[i] = '\0';
        } else {
            // Assume variable name
            char var_name[64];
            int i = 0;
            while (*p && *p != ';' && *p != ' ' && i < 63) {
                var_name[i++] = *p++;
            }
            var_name[i] = '\0';
            const char *val = capi_get_variable(var_name);
            if (val) {
                strncpy(reply, val, sizeof(reply) - 1);
            }
        }

        send(current_client_sock, reply, strlen(reply), 0);
        close(current_client_sock);
        current_client_sock = -1;
        
        strncat(res->output, "Reply sent and connection closed.\n", sizeof(res->output) - strlen(res->output) - 1);
        return 1;
    }

    return 0;
}

void capi_init(void) {
    capi_register_command("socket.", socket_handler);
}
