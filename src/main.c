#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>


#define PORT 8080

int main()
{
    int server_fd,client_fd;

    struct sockaddr_in address;
    memset(&address,0,sizeof(address));

    int len_addr=sizeof(address);

    char *msg="HTTP/1.1 200 OK/\n\n Reverse Proxy!\n";

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

    printf("Client conectat ! File descriptor client : %d\n",client_fd);
    write(client_fd,msg,strlen(msg));

    close(client_fd);
    }

    
    close(server_fd);


    return 0;
}
