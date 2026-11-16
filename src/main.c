#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "../include/server.h"

#define PORT 8080
#define HEALTH_CHECK_INTERVAL 10

int reload_request=0;
int keep_running=1;
int health_check_request=0;
int stats_request=0;

void handle_sighup(int sig)
{
    (void)sig;
    reload_request=1;
}

void handle_sigint(int sig)
{
    (void)sig;
    keep_running=0;
}

void handle_sigalrm(int sig)
{
    (void)sig;
    health_check_request=1;
}

void handle_sigusr1(int sig)
{
    (void)sig;
    stats_request=1;
}

int main()
{
    /* Alocam structura LoadBalancer in memorie partajata (mmap) pentru ca
       procesele copil sa poata actualiza active_connections in timp real */
    LoadBalancer *lb = mmap(NULL, sizeof(LoadBalancer),
                            PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(lb == MAP_FAILED)
    {
        perror("Eroare la mmap pentru LoadBalancer!");
        exit(-1);
    }

    load_config(lb, "config/config.txt");
    check_backends_health(lb);

    int server_fd, client_fd;

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));

    struct sigaction sa;
    sa.sa_flags=0;
    sigemptyset(&sa.sa_mask);

    sa.sa_handler=handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler=handle_sighup;
    sigaction(SIGHUP, &sa, NULL);

    sa.sa_handler=handle_sigalrm;
    sigaction(SIGALRM, &sa, NULL);

    sa.sa_handler=handle_sigusr1;
    sigaction(SIGUSR1, &sa, NULL);

    signal(SIGCHLD, SIG_IGN);

    alarm(HEALTH_CHECK_INTERVAL);

    int stats_pipe[2];
    if(pipe(stats_pipe) < 0)
    {
        perror("Eroare la crearea Pipe-ului!");
        munmap(lb, sizeof(LoadBalancer));
        exit(-1);
    }

    int flags=fcntl(stats_pipe[0], F_GETFL, 0);
    fcntl(stats_pipe[0], F_SETFL, flags | O_NONBLOCK);

    server_fd=socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0)
    {
        perror("Eroare la crearea socket-ului server!");
        munmap(lb, sizeof(LoadBalancer));
        exit(-1);
    }

    int opt=1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family=AF_INET;
    address.sin_addr.s_addr=INADDR_ANY;
    address.sin_port=htons(PORT);

    if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
    {
        perror("Eroare la bind!");
        close(server_fd);
        munmap(lb, sizeof(LoadBalancer));
        exit(-1);
    }

    if(listen(server_fd, 10) < 0)
    {
        perror("Eroare la listen!");
        close(server_fd);
        munmap(lb, sizeof(LoadBalancer));
        exit(-1);
    }

    printf("Serverul asculta pe Portul(%d)...\n", PORT);

    int total_request=0;

    while(keep_running)
    {
        StatsMessage stat_msg;
        while(read(stats_pipe[0], &stat_msg, sizeof(stat_msg)) == sizeof(stat_msg))
        {
            total_request++;

            /* Daca backend-ul ales initial nu a putut raspunde, il marcam OFFLINE direct in parinte */
            if(stat_msg.preferred_idx >= 0 && stat_msg.preferred_idx < lb->count &&
               stat_msg.used_idx != stat_msg.preferred_idx)
            {
                lb->servers[stat_msg.preferred_idx].is_alive = 0;
                printf("[PIPE IPC] Backend-ul %s:%d a picat! Marcat OFFLINE in procesul parinte.\n",
                       lb->servers[stat_msg.preferred_idx].ip,
                       lb->servers[stat_msg.preferred_idx].port);
            }

            if(stat_msg.used_idx >= 0 && stat_msg.used_idx < lb->count)
            {
                lb->servers[stat_msg.used_idx].requests_served++;
                printf("[PIPE STATS] Total cereri: %d | Servit de %s:%d (cereri pe acest backend: %d)\n",
                       total_request,
                       lb->servers[stat_msg.used_idx].ip,
                       lb->servers[stat_msg.used_idx].port,
                       lb->servers[stat_msg.used_idx].requests_served);
            }
            else
            {
                printf("[PIPE STATS] Total cereri: %d | Cerere respinsa (503 - niciun backend viu)\n",
                       total_request);
            }
        }

        if(reload_request)
        {
            printf("\n[INFO SIGHUP] Reload config.txt... \n");
            load_config(lb, "config/config.txt");

            check_backends_health(lb); //load_config pune is_alive = 0, deci reverificam

            for(int i = 0; i < lb->count; i++)
            {
                printf("[Backend Updated] %d IP %s | Port %d\n", i+1, lb->servers[i].ip, lb->servers[i].port);
            }

            reload_request=0;
        }

        if(health_check_request)
        {
            printf("\n[INFO SIGALRM] Health Check periodic...\n");
            check_backends_health(lb);
            health_check_request=0;
            alarm(HEALTH_CHECK_INTERVAL);
        }

        if(stats_request)
        {
            print_stats(lb, total_request);
            stats_request=0;
        }

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

        if(client_fd < 0)
        {
            if(errno == EINTR)
            {
                continue; // accept() a fost intrerupt de un semnal (SIGINT, SIGHUP sau SIGALRM)
            }
            perror("Eroare la accept!");
            continue;
        }

        //alegem backend-ul in PARINTE, ca sa pastreze curent_idx intre cereri
        char client_ip[INET_ADDRSTRLEN]="?";
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int backend_idx=select_backend(lb, client_ip);

        if(backend_idx >= 0)
        {
            printf("[BALANCER] Client %s -> backend %s:%d (active: %d)\n",
                   client_ip,
                   lb->servers[backend_idx].ip,
                   lb->servers[backend_idx].port,
                   lb->servers[backend_idx].active_connections);
        }
        else
        {
            printf("[BALANCER] Client %s -> niciun backend disponibil!\n", client_ip);
        }

        int pid=fork();

        switch (pid)
        {
        case -1:
            perror("Eroare la fork");
            close(client_fd);
            continue;

        case 0:
            alarm(0); // copilul nu are nevoie de alarma de health check
            close(stats_pipe[0]);
            close(server_fd);
            printf("\n\nProces copil (PID: %d), serveste clientul (FD::%d)\n", getpid(), client_fd);

            char buffer[4096]={0};
            int bytes_read = read(client_fd, buffer, sizeof(buffer)-1);

            if(bytes_read > 0)
            {
                printf("\n\n----------Cerere Primita ---\n%s\n---------------\n", buffer);

                int used_backend = -1;
                if(backend_idx >= 0)
                {
                    used_backend = forward_to_backend(lb, backend_idx, client_fd, buffer, bytes_read);
                }

                if(used_backend < 0)
                {
                    char *msg = "HTTP/1.1 503 Service Unavailable\r\nContent-Type: text/plain\r\n\r\nNiciun backend disponibil\n";
                    write(client_fd, msg, strlen(msg));
                }

                /* Scriem in proxy.log cu apeluri de sistem I/O */
                log_request(client_ip, lb, backend_idx, used_backend);

                /* Trimitem rezultatul catre parinte prin pipe */
                StatsMessage child_stat;
                child_stat.preferred_idx = backend_idx;
                child_stat.used_idx = used_backend;
                write(stats_pipe[1], &child_stat, sizeof(child_stat));
            }

            close(stats_pipe[1]);
            close(client_fd);
            _exit(0);

        default:
            close(client_fd);
            break;
        }
    }

    printf("\n\n[SHUTDOWN] SigInt primit!\n");
    print_stats(lb, total_request);
    printf("[SHUTDOWN] Se elibereaza resursele...");

    close(stats_pipe[0]);
    close(stats_pipe[1]);
    close(server_fd);
    munmap(lb, sizeof(LoadBalancer));

    printf("\n\n[SHUTDOWN] All good!\n");

    return 0;
}
