#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "../include/server.h"

#define PORT 8080

int reload_request=0;
int keep_running=1;

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

int main()
{

    LoadBalancer lb;
    load_config(&lb,"config/config.txt");
    check_backends_health(&lb);


    int server_fd,client_fd;

    struct sockaddr_in address;
    memset(&address,0,sizeof(address));

	//comentata pentru ca nu mai este folosita
	//iar compilatorul da warning
    //int len_addr=sizeof(address);


    struct sigaction sa;
    sa.sa_flags=0;
    sigemptyset(&sa.sa_mask);

    sa.sa_handler=handle_sigint;
    sigaction(SIGINT,&sa,NULL);

    sa.sa_handler=handle_sighup;
    sigaction(SIGHUP,&sa,NULL);

    signal(SIGCHLD,SIG_IGN);

    int stats_pipe[2];
    if(pipe(stats_pipe)<0)
    {
        perror("Eroare la crearea Pipe-ului!");
        exit(-1);
    }


    int flags=fcntl(stats_pipe[0],F_GETFL,0);
    fcntl(stats_pipe[0],F_SETFL,flags | O_NONBLOCK);


    server_fd=socket(AF_INET,SOCK_STREAM,0); 

    int opt=1;
    setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    address.sin_family=AF_INET;
    address.sin_addr.s_addr=INADDR_ANY;
    address.sin_port=htons(PORT);

    bind(server_fd,(struct sockaddr*)&address,sizeof(address));

    listen(server_fd,3);

    printf("Serverul asculta pe Portul(%d)...\n",PORT);

    int total_request=0;

    while(keep_running)
    {
        char dummy;
        while(read(stats_pipe[0],&dummy,1)>0)
        {
            total_request++;
            printf("[PIPE STATS] Total cereri servite pana acum %d\n",total_request);
        }


    if(reload_request)
    {
        printf("\n[INFO SIGHUP] Reload config.txt... \n");
        load_config(&lb,"config/config.txt");

		check_backends_health(&lb); //load_config pune is_alive = 0, deci reverificam

        for(int i = 0 ;i<lb.count;i++)
        {
            printf("[Backend Updated] %d IP %s | Port %d\n",i+1,lb.servers[i].ip,lb.servers[i].port);

        }
        
        reload_request=0;
    }

    //client_fd=accept(server_fd,(struct sockaddr*)&address,(socklen_t*)&len_addr);

	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

    if(client_fd<0)
    {
        if(errno==EINTR)
        {
            continue; // a fost apasat ctrl+c ,keep_running ->0 si o sa se iasa din while  
        }
        perror("Eroare la accept!");
        continue;
    }

	//alegem backend ul in PARINTE, casa pastreze curent_idx intre cereri
	char client_ip[INET_ADDRSTRLEN]="?";
    inet_ntop(AF_INET,&client_addr.sin_addr,client_ip,sizeof(client_ip));
    int backend_idx=select_backend(&lb,client_ip);

    if(backend_idx>=0)
    {
        printf("[BALANCER] Client %s -> backend %s:%d\n",client_ip,lb.servers[backend_idx].ip,lb.servers[backend_idx].port);
    }
    else
    {
        printf("[BALANCER] Client %s -> niciun backend disponibil!\n",client_ip);
    }

    int pid=fork();

   
    switch (pid)
    {
    case -1:
        
        perror("Eroare la fork");
        close(client_fd);
        continue;
    
    case 0:
        close(stats_pipe[0]);
        close(server_fd);
        printf("\n\nProces copil (PID: %d), serveste clientul (FD::%d)\n",getpid(),client_fd);
        
        char buffer[1024]={0};
        read(client_fd,buffer,sizeof(buffer)-1);

        printf("\n\n----------Cerere Primita ---\n%s\n---------------\n",buffer);

        //char *msg="HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nReverse Proxy works!\n";
                char msg[256];
        if(backend_idx>=0)
        {
            snprintf(msg,sizeof(msg),"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nReverse Proxy works! Backend ales: %s:%d\n",lb.servers[backend_idx].ip,lb.servers[backend_idx].port);
        }
        else
        {
            snprintf(msg,sizeof(msg),"HTTP/1.1 503 Service Unavailable\r\nContent-Type: text/plain\r\n\r\nNiciun backend disponibil\n");
        }
		write(client_fd,msg,strlen(msg));


        char stat_signal='1';
        write(stats_pipe[1],&stat_signal,1);
        close(stats_pipe[1]);


        close(client_fd);
        _exit(0);

    default:
        close(client_fd);
        break;
    }

    
    }


    printf("\n\n[SHUTDOWN] SigInt primit!");
    printf("\n\n[SHUTDOWN] Se elibereaza resursele...");

    close(stats_pipe[0]);
    close(stats_pipe[1]);
    close(server_fd);

    printf("\n\n[SHUTDOWN] All good!");


    return 0;
}
