#ifndef Process_H
#define Process_H

#include <vector>
#include <string>

using namespace std;
class Process
{
    public:
        Process(char*,char**,vector<string>,int);
        char* executable;
        char** argv;
        vector<string> envp;
        int input;
        int output;
        int err;
        int iosocket;
        int run();
};

#endif