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

class Userpipe{
    public:
        bool used;
        int from_user_id;
        int to_user_id;
        int thepipe[2];
        vector<int> proc_pids; // if no number pipe continue need to wait all of them
        Userpipe(){
            used = false;
            from_user_id=-1;
            to_user_id=-1;
            thepipe[0]=-1;
            thepipe[1]=-1;
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
        Userpipe userpipes_to_me[31]; // reuse number pipe  number = from_user_id
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
        vector<pid_t> is_wait_for;
};

#endif