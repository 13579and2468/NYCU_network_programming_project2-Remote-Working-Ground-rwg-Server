#ifndef NPshell_H
#define NPshell_H

#include <string>

using namespace std;
class NPshell
{
    public:
        NPshell();
        int run();
        string name;
        void init();
        void broadcast(string s);
        void tell(int user,string s);
        int shmfilefd;
        static void* receive_shmfile;
        static void get_message_handler(int sig);
};

#endif