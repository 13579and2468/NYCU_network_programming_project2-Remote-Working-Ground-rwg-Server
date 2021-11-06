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

int main(int argc, char **argv,char **envp)
{
	char	*service;	/* service name or port number	*/
	socklen_t	alen;			/* length of client's address	*/
	int	msock;			/* master server socket		*/
	fd_set	rfds;		/* read file descriptor set	*/
    fd_set	afds;		/* active file descriptor set */
    int	fd, nfds;
	NPshell** users = new NPshell*[2000];
	int userused[31] = {0};
	NPshell** get_user_by_id = new NPshell*[31];

	switch (argc) {
	case	2:
		service = argv[1];
		break;
	default:
		errexit("usage: ./np_simple [port]\n");
	}

	msock = passiveTCP(service, QLEN);
	nfds = getdtablesize();
    FD_ZERO(&afds);
    FD_SET(msock, &afds);

	signal(SIGCHLD, reaper);

	while (1) {
		memcpy(&rfds, &afds, sizeof(rfds));

		if (select(nfds, &rfds, (fd_set *)0, (fd_set *)0,(struct timeval *)0) < 0)errexit("select: %s\n", strerror(errno));
		if (FD_ISSET(msock, &rfds))
		{
			sockaddr_in fsin;	/* the address of a client	*/
            int	ssock;
            alen = sizeof(fsin);
            ssock = accept(msock, (struct sockaddr *)&fsin,&alen);
            if (ssock < 0)errexit("accept: %s\n", strerror(errno));
            FD_SET(ssock, &afds);
			int userid;
			int i;
			for (i=1;i<31;i++)
			{
				if(userused[i]==0)
				{
					userid = i;
					userused[i]=1;
					break;
				}
			}
			users[ssock] = new NPshell(ssock,envp,fsin,&afds,msock,userid,userused,get_user_by_id);
			get_user_by_id[i] = users[ssock];
        }

        for (fd=0; fd<nfds; ++fd)
		{
            if (fd != msock && FD_ISSET(fd, &rfds))
			{
				char buffer[20000];

				if(readLine(fd,buffer,20000)==0)
				{
					close(fd);
					FD_CLR(fd, &afds);
					userused[users[fd]->userid]=0;
					users[fd]->broadcast("*** User '"+users[fd]->name+"' left. ***\n");
					delete users[fd];
					continue;
				}

				if(users[fd]->runline(string(buffer))==2)  // return meeans user use exit
				{
					close(fd);
					FD_CLR(fd, &afds);
					userused[users[fd]->userid]=0;
					users[fd]->broadcast("*** User '"+users[fd]->name+"' left. ***\n");
					delete users[fd];
				}
			}
		}
	}
	delete[] users;
	delete[] get_user_by_id;
}

/*------------------------------------------------------------------------
 * reaper - clean up zombie children
 *------------------------------------------------------------------------
 */
/*ARGSUSED*/
void reaper(int sig)
{
	int	status;

	while (wait3(&status, WNOHANG, (struct rusage *)0) >= 0)
		/* empty */;
}