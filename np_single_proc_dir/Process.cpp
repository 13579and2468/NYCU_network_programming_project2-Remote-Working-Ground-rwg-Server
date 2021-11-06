#include "Process.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <exception>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <vector>

using namespace std;

Process::Process(char* executable,char** argv,vector<string> envp,int iosocket)
{
    this->executable = executable;
    this->argv = argv;
    this->input = -1;  // -1 means no change
    this->output = -1;
    this->err = -1;
    this->iosocket = iosocket;
    this->envp = envp;
}

int Process::run()
{
    //setenv because only use execvpe with environment doesn't affect PATH
    for(auto s: envp)
    {
      setenv(s.substr(0,s.find("=")).c_str(),s.substr(s.find("=")+1).c_str(),1);
    }

    if(this->err!=-1)
    {
        dup2(this->err,STDERR_FILENO);
    }else
    {
        dup2(iosocket,STDERR_FILENO);
    }

    if(this->input!=-1)
    {
        dup2(this->input,STDIN_FILENO);
    }else
    {
        dup2(iosocket,STDIN_FILENO);
    }

    if(this->output!=-1)
    {
        dup2(this->output,STDOUT_FILENO);
    }else
    {
        dup2(iosocket,STDOUT_FILENO);
    }
    // close(-1) do nothing
    close(this->err);
    close(this->input);
    close(this->output);
    close(iosocket);


    // file redirection
    for (int i=0;argv[i]!=NULL;i++)
    {
        if(strcmp(argv[i],">")==0)
        {
            int fd;
            try{
                fd = open( argv[i+1],O_CREAT|O_RDWR|O_TRUNC,S_IRWXU|S_IRWXG|S_IRWXO );
            }
            catch(...){
                std::cerr<< "open file failed\n";
                exit(0);
            }
            dup2(fd,STDOUT_FILENO);
            close(fd);
            argv[i]=NULL;
            break;
        }
    }

    int r = 0;
    if((r = execvp(executable,argv))==-1)
    {
        delete[] argv;
        std::cerr<<"Unknown command: ["<<executable<<"].\n";
        exit(0);
    }

    return r;
}