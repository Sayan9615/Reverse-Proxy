#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "../include/server.h"


void load_config(LoadBalancer *lb,const char *filepath)
{

    lb->count=0;
    lb->curent_idx=0;

    int fd=open(filepath,O_RDONLY);
    if(fd<0)
    {
        perror("Eroare la deschidere config.txt....");
        exit(-1);
    }

    char buffer[1024];
    int bytes_read=read(fd,buffer,sizeof(buffer)-1);\
    buffer[bytes_read]='\0';
    close(fd);


    for (char *line =strtok(buffer,"\n");line!=NULL && lb->count<MAX_BACKEND_SVS;line=strtok(NULL,"\n"))
    {
        char *space_ptr=strchr(line,' ');

        if(space_ptr !=NULL)
        {
            *space_ptr='\0';
            strncpy(lb->servers[lb->count].ip,line,63);
            lb->servers[lb->count].ip[63]='\0';

            lb->servers[lb->count].port=atoi(space_ptr+1);
            lb->servers[lb->count].is_alive=0;

            lb->count++;

        }

    }



}

