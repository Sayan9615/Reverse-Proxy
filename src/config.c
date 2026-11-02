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
	lb->algo=ALGO_ROUND_ROBIN; //implicit, daca lipseste linia ALGORITHM

    int fd=open(filepath,O_RDONLY);
    if(fd<0)
    {
        perror("Eroare la deschidere config.txt....");
        exit(-1);
    }

    char buffer[1024];
    int bytes_read=read(fd,buffer,sizeof(buffer)-1);
	if(bytes_read < 0){
		perror("Eroare la citirea config.txt...\nExitting...");
		close(fd);
		exit(-1);
	}
    buffer[bytes_read]='\0';
    close(fd);


    for (char *line =strtok(buffer,"\n");line!=NULL && lb->count<MAX_BACKEND_SVS;line=strtok(NULL,"\n"))
    {
		if(line[0]=='#'){	//comentariile
			continue;
		}

		if(strncmp(line, "ALGORITHM ", 10) == 0){
			char *value = line+10;
			value[strcspn(value, " \r\n")]='\0';

			if(strcmp(value, "ip_hash") == 0){
				lb->algo = ALGO_IP_HASH;
			}
			else if(strcmp(value, "round_robin") == 0){
				lb->algo = ALGO_ROUND_ROBIN;
			}
			else{
				fprintf(stderr, "Algirm necunoscut in config: %s (folosesc round_robin implicit...)\n", value);
			}
			continue;
		}
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

