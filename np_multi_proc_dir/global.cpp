#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h> 

sockaddr_in client_addr;
int userid;
bool* userused = (bool*)mmap(NULL,31*sizeof(bool),PROT_READ | PROT_WRITE,MAP_SHARED | MAP_ANONYMOUS,-1,0);
int* userid_to_pid = (int*)mmap(NULL,31*sizeof(int),PROT_READ | PROT_WRITE,MAP_SHARED | MAP_ANONYMOUS,-1,0);