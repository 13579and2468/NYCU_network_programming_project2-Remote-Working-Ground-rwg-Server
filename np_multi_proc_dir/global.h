#include <netinet/in.h>

#ifndef GLOBAL_H
#define GLOBAL_H

extern int userid;
extern bool* userused;
extern int* userid_to_pid;
extern char** usernames;
extern sockaddr_in** client_addrs; 
#endif

