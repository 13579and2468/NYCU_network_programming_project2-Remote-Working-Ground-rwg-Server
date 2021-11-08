#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h> 

int userid;
bool* userused = (bool*)mmap(NULL,31*sizeof(bool),PROT_READ | PROT_WRITE,MAP_SHARED | MAP_ANONYMOUS,-1,0);
int* userid_to_pid = (int*)mmap(NULL,31*sizeof(int),PROT_READ | PROT_WRITE,MAP_SHARED | MAP_ANONYMOUS,-1,0);
char **usernames =  (char**)mmap(NULL,31*sizeof(char*),PROT_READ | PROT_WRITE,MAP_SHARED | MAP_ANONYMOUS,-1,0);
sockaddr_in** client_addrs = (sockaddr_in**)mmap(NULL,31*sizeof(sockaddr_in*),PROT_READ | PROT_WRITE,MAP_SHARED | MAP_ANONYMOUS,-1,0);