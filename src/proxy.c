#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../include/server.h"

/* Trimite cererea clientului catre backend-ul preferat (preferred_idx).
   Daca acel backend a picat intre timp, face FAILOVER automat si incearca
   celelalte backend-uri din lista.
   Intoarce indexul backend-ului care a raspuns, sau -1 daca toate au esuat. */
int forward_to_backend(LoadBalancer *lb, int preferred_idx, int client_fd, const char *request, int req_len)
{
    if (lb->count <= 0 || preferred_idx < 0)
    {
        return -1;
    }

    for (int attempt = 0; attempt < lb->count; attempt++)
    {
        int idx = (preferred_idx + attempt) % lb->count;

        /* La failover (attempt > 0), sarim peste serverele deja stiute ca fiind picate */
        if (attempt > 0 && !lb->servers[idx].is_alive)
        {
            continue;
        }

        int backend_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (backend_fd < 0)
        {
            perror("Eroare la crearea socket-ului catre backend");
            continue;
        }

        struct sockaddr_in backend_addr;
        memset(&backend_addr, 0, sizeof(backend_addr));
        backend_addr.sin_family = AF_INET;
        backend_addr.sin_port = htons(lb->servers[idx].port);
        inet_pton(AF_INET, lb->servers[idx].ip, &backend_addr.sin_addr);

        if (connect(backend_fd, (struct sockaddr *)&backend_addr, sizeof(backend_addr)) < 0)
        {
            printf("[FAILOVER] Backend-ul %s:%d nu raspunde! Se cauta altul...\n",
                   lb->servers[idx].ip, lb->servers[idx].port);
            close(backend_fd);
            continue;
        }

        if (attempt > 0)
        {
            printf("[FAILOVER] Cerere redirectionata cu succes catre %s:%d\n",
                   lb->servers[idx].ip, lb->servers[idx].port);
        }

        /* 1. Trimitem cererea primita de la client catre backend */
        if (write(backend_fd, request, req_len) < 0)
        {
            perror("Eroare la trimiterea cererii catre backend");
            close(backend_fd);
            continue;
        }

        /* 2. Citim raspunsul de la backend si il trimitem inapoi la client */
        char response_buf[4096];
        int bytes;
        while ((bytes = read(backend_fd, response_buf, sizeof(response_buf))) > 0)
        {
            write(client_fd, response_buf, bytes);
        }

        close(backend_fd);
        return idx;
    }

    return -1;
}
