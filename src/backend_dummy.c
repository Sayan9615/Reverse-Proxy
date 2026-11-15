#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

static int keep_running = 1;

void handle_sigint(int sig)
{
    (void)sig;
    keep_running = 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Utilizare: %s <port>\n", argv[0]);
        fprintf(stderr, "Exemplu:   %s 8081\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535)
    {
        fprintf(stderr, "Port invalid: %s\n", argv[1]);
        return 1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("Eroare la crearea socket-ului");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Eroare la bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 5) < 0)
    {
        perror("Eroare la listen");
        close(server_fd);
        return 1;
    }

    printf("====================================================\n");
    printf("[BACKEND %d] Serverul ruleaza pe portul %d...\n", port, port);
    printf("[BACKEND %d] Apasa Ctrl+C pentru a simula oprirea (picajul) serverului.\n", port);
    printf("====================================================\n");

    while (keep_running)
    {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);

        if (client_fd < 0)
        {
            continue;
        }

        char buffer[1024] = {0};
        int bytes = read(client_fd, buffer, sizeof(buffer) - 1);

        /* Daca e doar health check (nu a trimis date sau s-a deconectat imediat) */
        if (bytes <= 0)
        {
            printf("[BACKEND %d] Ping primit (Health Check)\n", port);
            close(client_fd);
            continue;
        }

        char client_ip[INET_ADDRSTRLEN] = "?";
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("[BACKEND %d] Cerere primita de la %s (%d bytes)\n", port, client_ip, bytes);

        char response[512];
        int resp_len = snprintf(response, sizeof(response),
                                "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/plain\r\n"
                                "Connection: close\r\n\r\n"
                                "Salut! Ai primit raspuns de la BACKEND-ul de pe portul %d!\n",
                                port);

        write(client_fd, response, resp_len);
        close(client_fd);
    }

    close(server_fd);
    printf("\n[BACKEND %d] Server oprit cu succes.\n", port);
    return 0;
}
