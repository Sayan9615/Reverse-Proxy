#include <stdio.h>
#include "../include/server.h"

/* Hash djb2 pe un string (aici: IP-ul clientului) */
static unsigned long hash_djb2(const char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = (unsigned char)*str++) != 0)
    {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

/* Round-Robin: pornim de la curent_idx si cautam primul backend viu */
static int select_round_robin(LoadBalancer *lb)
{
    for (int i = 0; i < lb->count; i++)
    {
        int idx = (lb->curent_idx + i) % lb->count;

        if (lb->servers[idx].is_alive)
        {
            lb->curent_idx = (idx + 1) % lb->count;
            return idx;
        }
    }

    return -1;
}

/* IP Hash: acelasi IP de client ajunge mereu la acelasi backend (cat timp
   setul de backend-uri vii nu se schimba). Hash-ul se calculeaza modulo
   numarul de backend-uri VII, apoi alegem al k-lea backend viu. */
static int select_ip_hash(LoadBalancer *lb, const char *client_ip)
{
    int alive = 0;

    for (int i = 0; i < lb->count; i++)
    {
        if (lb->servers[i].is_alive)
        {
            alive++;
        }
    }

    if (alive == 0)
    {
        return -1;
    }

    int k = (int)(hash_djb2(client_ip) % (unsigned long)alive);

    for (int i = 0; i < lb->count; i++)
    {
        if (lb->servers[i].is_alive)
        {
            if (k == 0)
            {
                return i;
            }
            k--;
        }
    }

    return -1; /* nu se ajunge aici */
}

/* Intoarce indexul backend-ului ales, sau -1 daca niciun backend nu e viu.
   Trebuie apelata in procesul PARINTE (inainte de fork), ca modificarea lui
   curent_idx sa se pastreze intre cereri. */
int select_backend(LoadBalancer *lb, const char *client_ip)
{
    if (lb->count <= 0)
    {
        return -1;
    }

    switch (lb->algo)
    {
    case ALGO_IP_HASH:
        return select_ip_hash(lb, client_ip);

    case ALGO_ROUND_ROBIN:
    default:
        return select_round_robin(lb);
    }
}
