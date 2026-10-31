#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../include/server.h"
#include <unistd.h>

void check_backends_health(LoadBalancer *lb)
{

    printf("------Verificare Status Backend-uri -------\n");

    for (int i =0; i<lb->count;i++)
    {

        int sock=socket(AF_INET,SOCK_STREAM,0);
        if(sock <0)
        {
            lb->servers[i].is_alive=0;
            continue;
        }

        struct sockaddr_in addr;
        addr.sin_family=AF_INET;
        addr.sin_port=htons(lb->servers[i].port);
        inet_pton(AF_INET,lb->servers[i].ip,&addr.sin_addr);

        if(connect(sock,(struct sockaddr*)&addr,sizeof(addr))==0)
        {
            lb->servers[i].is_alive=1;
            printf("[ONLINE] %s:%d\n",lb->servers[i].ip,lb->servers[i].port);
        }
        else
        {
            lb->servers[i].is_alive=0;
            printf("[OFFLINE] %s:%d\n",lb->servers[i].ip,lb->servers[i].port);
            
        }

        close(sock);

    }

    printf("----------------------------------");

}