#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "../include/server.h"

#define LOG_FILE "proxy.log"

/* Scrie in fisierul proxy.log folosind apeluri de sistem POSIX (open, write, close) */
void log_request(const char *client_ip, LoadBalancer *lb, int preferred_idx, int used_idx)
{
    int fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
    {
        perror("Eroare la deschiderea proxy.log");
        return;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    char log_line[512];
    int len = 0;

    if (used_idx >= 0 && used_idx == preferred_idx)
    {
        len = snprintf(log_line, sizeof(log_line),
                       "[%s] Client %s -> Backend %s:%d [OK]\n",
                       time_str, client_ip,
                       lb->servers[used_idx].ip, lb->servers[used_idx].port);
    }
    else if (used_idx >= 0 && preferred_idx >= 0 && used_idx != preferred_idx)
    {
        len = snprintf(log_line, sizeof(log_line),
                       "[%s] Client %s -> FAILOVER %s:%d -> %s:%d [RECOVERED]\n",
                       time_str, client_ip,
                       lb->servers[preferred_idx].ip, lb->servers[preferred_idx].port,
                       lb->servers[used_idx].ip, lb->servers[used_idx].port);
    }
    else
    {
        len = snprintf(log_line, sizeof(log_line),
                       "[%s] Client %s -> Niciun backend disponibil [503 ERROR]\n",
                       time_str, client_ip);
    }

    if (len > 0)
    {
        write(fd, log_line, len);
    }

    close(fd);
}
