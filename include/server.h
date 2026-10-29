#ifndef SERVER_H
#define SERVER_H

#define MAX_BACKEND_SVS 10

typedef struct 
{
    char ip[64];
    int port;
    int is_alive;  //activ = 1 ,activ=0 (pentru Health check)

}BackendServer;


typedef struct 
{
    BackendServer servers[MAX_BACKEND_SVS];
    int count;
    int curent_idx; // (pentru round robin)
    
}LoadBalancer;


void load_config(LoadBalancer *lb,const char *filepath);
void check_backends_health(LoadBalancer *lb);


#endif