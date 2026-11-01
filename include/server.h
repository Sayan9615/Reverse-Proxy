#ifndef SERVER_H
#define SERVER_H

#define MAX_BACKEND_SVS 10

typedef struct 
{
    char ip[64];
    int port;
    int is_alive;  //activ = 1 ,activ=0 (pentru Health check)

}BackendServer;

//definirea structurii algoritmului
typedef enum
{
	ALGO_ROUND_ROBIN = 0,
	ALGO_IP_HASH = 1
}Algorithm;

typedef struct 
{
    BackendServer servers[MAX_BACKEND_SVS];
    int count;
    int curent_idx; // (pentru round robin)
	Algorithm algo; // algoritmul de load balancing (din config.txt)
    
}LoadBalancer;


void load_config(LoadBalancer *lb,const char *filepath);
void check_backends_health(LoadBalancer *lb);
int select_backend(LoadBalancer *lb, const char *client_ip); //-1 daca nu exista backend viu

#endif
