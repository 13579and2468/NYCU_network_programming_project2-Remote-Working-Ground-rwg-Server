#include <netinet/in.h>

#ifndef GLOBAL_H
#define GLOBAL_H

extern int userid;
extern bool* userused;
extern pid_t* userid_to_pid;
extern char** usernames;
extern sockaddr_in** client_addrs; 
extern bool* userpipes_used;  //[user_pipe_from+31*userid]
#endif

