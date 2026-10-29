#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include "../include/server.h"

#define PORT 8080

int main()
{

    LoadBalancer lb;
    load_config(&lb,"config/config.txt");
     
    //TODO :check_backends_health(&lb)


    int server_fd,client_fd;

    struct sockaddr_in address;
    memset(&address,0,sizeof(address));

    int len_addr=sizeof(address);


    signal(SIGCHLD,SIG_IGN);

    server_fd=socket(AF_INET,SOCK_STREAM,0); 

    address.sin_family=AF_INET;
    address.sin_addr.s_addr=INADDR_ANY;
    address.sin_port=htons(PORT);

    bind(server_fd,(struct sockaddr*)&address,sizeof(address));

    listen(server_fd,3);

    printf("Serverul asculta pe Portul(%d)...\n",PORT);

    while(1)
    {
    client_fd=accept(server_fd,(struct sockaddr*)&address,(socklen_t*)&len_addr);

    int pid=fork();

   
    switch (pid)
    {
    case -1:
        
        perror("Eroare la fork");
        close(client_fd);
        continue;
    
    case 0:
        close(server_fd);
        printf("Proces copil (PID: %d), serveste clientul (FD::%d)\n",getpid(),client_fd);
        
        char buffer[1024]={0};
        read(client_fd,buffer,sizeof(buffer)-1);

        printf("----------Cerere Primita ---\n%s\n---------------\n",buffer);

        char *msg="HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nReverse Proxy works!\n";
        write(client_fd,msg,strlen(msg));

        close(client_fd);
        _exit(0);

    default:
        close(client_fd);
        break;
    }

    
    }

    
    close(server_fd);


    return 0;
}
