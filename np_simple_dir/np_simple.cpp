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

#define	QLEN		   5	/* maximum connection queue length	*/
#define	BUFSIZE		4096

extern int	errno;

using namespace std;
void	reaper(int);

int main(int argc, char *argv[])
{
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

	while (1) {
		alen = sizeof(fsin);
		ssock = accept(msock, (struct sockaddr *)&fsin, &alen);
		if (ssock < 0) {
			if (errno == EINTR)
				continue;
			errexit("accept: %s\n", strerror(errno));
		}
		switch (fork()) {
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
		}
        close(ssock);
	}
}