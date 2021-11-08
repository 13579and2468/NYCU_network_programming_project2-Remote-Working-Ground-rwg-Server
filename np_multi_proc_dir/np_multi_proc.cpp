#include "NPshell.h"
#include "../shared_lib/Mysocket.cpp"

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include "global.h"

#define	QLEN		   5	/* maximum connection queue length	*/

extern int	errno;

using namespace std;
void	reaper(int);

int main(int argc, char *argv[])
{
	//setting SIGCHILD tp ignore will give zombie to init process
  	signal(SIGCHLD,SIG_IGN);
	char	*service;	/* service name or port number	*/
	struct	sockaddr_in fsin;	/* the address of a client	*/
	socklen_t	alen;			/* length of client's address	*/
	int	msock;			/* master server socket		*/
	int	ssock;			/* slave server socket		*/

	switch (argc) {
	case	1:
		exit(1);
	case	2:
		service = argv[1];
		break;
	default:
		errexit("usage: ./np_simple [port]\n");
	}

	msock = passiveTCP(service, QLEN);

	//prepare shared memory for usernames and user sockaddr
	for(int i=1;i<31;i++)
	{
		client_addrs[i] = (sockaddr_in*)mmap(NULL,sizeof(sockaddr_in),PROT_READ | PROT_WRITE,MAP_SHARED | MAP_ANONYMOUS,-1,0);
		usernames[i] =  (char*)mmap(NULL,21*sizeof(char),PROT_READ | PROT_WRITE,MAP_SHARED | MAP_ANONYMOUS,-1,0); // name max 20 length
	}

	//clear userpipes flag
	for(int i=0;i<31*31;i++)
	{
		userpipes_used[i] = false;
	}

	while (1) {
		sockaddr_in fsin;
		alen = sizeof(fsin);
		ssock = accept(msock, (struct sockaddr *)&fsin, &alen);
		if (ssock < 0) {
			if (errno == EINTR)
				continue;
			errexit("accept: %s\n", strerror(errno));
		}
		for(int i=1;i<31;i++)
		{
			if(!userused[i])
			{
				memcpy(client_addrs[i],&fsin,sizeof(fsin));
				userused[i] = true;
				userid = i;
				break;
			}
		}
		switch (int pid = fork()) {
		case 0:		/* child */
            {
                close(msock);
                dup2(ssock,STDIN_FILENO);
                dup2(ssock,STDERR_FILENO);
                dup2(ssock,STDOUT_FILENO);
				NPshell shell;
                shell.run();
                exit(0);
                break;
            }
		case -1:
			errexit("fork: %s\n", strerror(errno));
            break;
		default:
			userid_to_pid[userid] = pid;
		}
        close(ssock);
	}
	for(int i=1;i<31;i++)
	{
		free(client_addrs[i]);
		free(usernames[i]);
	}
	free(client_addrs);
	free(usernames);

	return 0 ;
}