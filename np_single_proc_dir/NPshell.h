#ifndef NPshell_H
#define NPshell_H

#include <vector>
#include <string>
#include <netinet/in.h>

using namespace std;

struct Numberpipe{
  int afterline;
  int lastpipe[2];
  vector<int> proc_pids; // if no number pipe continue need to wait all of them

  operator int() const{
    return afterline;
  }
};

class NPshell
{
    public:
        NPshell();
        NPshell(int,char**,sockaddr_in,fd_set*,int,int,int[31],NPshell**);
        int runline(string line);
        string input;
        vector<Numberpipe> numberpipes;
        int mysocket;
        vector<string> myenvp;
        sockaddr_in client_addr;
        fd_set* afds;
        int nfds;
        int msock;
        int myprintenv(string s);
        int mysetenv(string var,string value);
        int userid;
        int* userused;
        NPshell** get_user_by_id;
        void broadcast(string);
        string name;
};

#endif