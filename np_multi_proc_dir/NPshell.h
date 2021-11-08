#ifndef NPshell_H
#define NPshell_H

#include <string>
#include <signal.h>

using namespace std;
class NPshell
{
    public:
        NPshell();
        int run();
        void init();
        void broadcast(string s);
        void tell(int user,string s);
        int shmfilefd;
        static void* receive_shmfile;
        static void get_message_handler(int sig);
        static void get_pipe_handler(int sig, siginfo_t *info, void *context);
        static int* FIFOtome;
};

#endif