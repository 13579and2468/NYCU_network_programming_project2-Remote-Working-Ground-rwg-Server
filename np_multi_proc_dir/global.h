#include <netinet/in.h>

#ifndef GLOBAL_H
#define GLOBAL_H

extern sockaddr_in client_addr;
extern int userid;
extern bool* userused;
extern int* userid_to_pid; 
#endif

