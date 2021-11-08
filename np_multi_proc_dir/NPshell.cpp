#include <iostream>
#include <vector>
#include <sstream>
#include <algorithm>
#include <unistd.h>
#include <csignal>
#include <sys/types.h> 
#include <sys/wait.h>
#include "Process.h"
#include "NPshell.h"
#include "global.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>    /* For O_RDWR */
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <cstring>
#include <sys/ioctl.h>


using namespace std;


void* NPshell::receive_shmfile;
int* NPshell::FIFOtome;

//create FIFO -> if exists delete
int createNewFifo( const char *fifoName )
{
    struct stat stats;
    if ( stat( fifoName, &stats ) < 0 )
    {
        if ( errno != ENOENT )          // ENOENT is ok, since we intend to delete the file anyways
        {
            perror( "stat failed" );    // any other error is a problem
            return( -1 );
        }
    }
    else                                // stat succeeded, so the file exists
    {
        if ( unlink( fifoName ) < 0 )   // attempt to delete the file
        {
            perror( "unlink failed" );  // the most likely error is EBUSY, indicating that some other process is using the file
            return( -1 );
        }
    }

    if ( mkfifo( fifoName, 0666 ) < 0 ) // attempt to create a brand new FIFO
    {
        perror( "mkfifo failed" );
        return( -1 );
    }

    return( 0 );
}

// split input into tokens
vector<string> split(string s){
  stringstream ss(s);
  string token;
  vector<string> v;
  while(ss>>token){
    v.push_back(token);
  }
  return v;
}

int myprintenv(string s){
  char *tmp = getenv(s.c_str());
  string env_var(tmp ? tmp : "");
  if (env_var.empty()) {
      return 0;
  }
  cout << env_var << endl;
  return 1;
}

int mysetenv(string var,string value){
  return setenv(var.c_str(),value.c_str(),1);
}

void NPshell::broadcast(string s)
{
  for(int i=1;i<31;i++)
  {
    if(userused[i])tell(i,s);
  }
}

void NPshell::tell(int user,string s)
{
  int target_shmfilefd = shm_open(to_string(user).c_str(),O_RDWR|O_CREAT,0644);
  void* target_shmfile = mmap(NULL,10240,PROT_READ | PROT_WRITE,MAP_SHARED,target_shmfilefd,0);
  ftruncate(target_shmfilefd,10240);
  memcpy(target_shmfile,s.c_str(),strlen(s.c_str())+1);
  kill(userid_to_pid[user],SIGUSR1);  // use 777 to knock receiver 
}

void NPshell::init(){
  mysetenv("PATH","bin:.");
  cout<<"****************************************\n** Welcome to the information server. **\n****************************************\n";
  memcpy(usernames[userid],"(no name)\x00",strlen("(no name)\x00")+1);
  stringstream ss;
  ss << "*** User '"<<usernames[userid]<<"' entered from " << inet_ntoa(client_addrs[userid]->sin_addr) << ":" << (int) ntohs(client_addrs[userid]->sin_port) << ". ***\n";
  broadcast(ss.str());
}

struct Numberpipe{
  int afterline;
  int lastpipe[2];
  vector<int> proc_pids; // if no number pipe continue need to wait all of them

  operator int() const{
    return afterline;
  }
};

int fork_unitil_success(){
  int r;
  while( (r=fork())==-1 );
  return r;
}

void NPshell::get_message_handler(int sig){
  cout<<(char*)receive_shmfile;
  fflush(stdout);
  *(char*)receive_shmfile = '\x00';
  signal(sig, get_message_handler);
}

void NPshell::get_pipe_handler(int sig, siginfo_t *info, void *context){ //use SIGUSR2
  pid_t senderid;
  for(senderid=1;senderid<31;senderid++)
  {
    if(userid_to_pid[senderid]==info->si_pid)break;
  }
  FIFOtome[senderid] = open(("user_pipe/"+to_string(senderid)+"to"+to_string(userid)).c_str(),O_RDONLY|O_NONBLOCK);
}


NPshell::NPshell(){
  shmfilefd = shm_open(to_string(userid).c_str(),O_RDWR|O_CREAT,0644);
  receive_shmfile = mmap(NULL,10240,PROT_READ | PROT_WRITE,MAP_SHARED,shmfilefd,0);
  ftruncate(shmfilefd,10240);

  //handle userFIFO read the userFIFO prevent sender block when open FIFO
  signal(SIGUSR1, get_message_handler);  // user SIGUSR1 to handle get message from other user or broadcast
  FIFOtome = new int[31];
  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = get_pipe_handler;
  sigaction(SIGUSR2,&sa,NULL);
}

int NPshell::run(){
  init();
  string input;
  vector<Numberpipe> numberpipes;
  while(true)
  {
    cout<<"% ";
    fflush(stdout);
    int count=0;

    while(!getline(cin,input))cin.clear();

    vector<string> tokens = split(input);
    if(tokens.size() == 0)continue;  // continue when empty line
    for (Numberpipe& p: numberpipes)p.afterline--;  // decrease all pipe line 

    // prepare if the line is piped
    Numberpipe pipe_to_this_line;
    bool linepiped = false;
    auto pipe_it = numberpipes.begin();
    if((pipe_it = find(numberpipes.begin(),numberpipes.end(),0))!=numberpipes.end())
    {
      linepiped = true;
      pipe_to_this_line = *pipe_it;
      numberpipes.erase(pipe_it);
    }

    //run commands
    auto it = tokens.begin();
    auto previt = it;
    vector<pid_t> childs;
    int prevpipe[2]={-1,-1};
    int thispipe[2]={-1,-1};
    //if numberpiped pipe to first command
    if(linepiped)
    {
      close(pipe_to_this_line.lastpipe[1]);
      thispipe[0]=pipe_to_this_line.lastpipe[0];
      childs = pipe_to_this_line.proc_pids;
    }

    // prepare if the line is userpiped
    //check if userpiped
    int user_pipe_from=-1;
    for(auto s:tokens)
    {
      if(s[0]=='<'&&s.length()!=1)
      {
        user_pipe_from = atoi( s.substr(1).c_str() );
        tokens.erase(find(tokens.begin(),tokens.end(),s));
        break;
      }
    }
    // there is userpipe to me
    if(user_pipe_from!=-1)
    {
      //user no exist
      if(user_pipe_from>30 || user_pipe_from<=0 || !userused[user_pipe_from])
      {
        printf("*** Error: user #%d does not exist yet. ***\n",user_pipe_from);
        thispipe[0]=open("/dev/null", O_RDONLY);;
      }else if(!userpipes_used[user_pipe_from+31*userid])
      {
        printf("*** Error: the pipe #%d->#%d does not exist yet. ***\n",user_pipe_from,userid);
        thispipe[0]=open("/dev/null", O_RDONLY);;
      }else
      {
        userpipes_used[user_pipe_from+31*userid] = false;
        thispipe[0] = FIFOtome[user_pipe_from];
        stringstream ss;
        ss<<"*** "<<usernames[userid]<<" (#"<<userid<<") just received from "<<usernames[user_pipe_from]<<" (#"<<user_pipe_from<<") by '"<<input.substr(0,min(input.find("\r"),input.find("\n")))<<"' ***\n";
        broadcast(ss.str());
      }
    }
    
    // built-in commands
    if(tokens[0] == "printenv")
    {
      if(tokens.size()==1)continue; //no environment var
      myprintenv(tokens[1]);
      continue;

    }else if(tokens[0]=="setenv")
    {
      if(tokens.size()<=2)continue; //no enough argument
      mysetenv(tokens[1],tokens[2]);
      continue;

    }else if(tokens[0]=="exit")
    {
      userused[userid] = false;
      shm_unlink(to_string(userid).c_str());
      broadcast(string("*** User '")+string(usernames[userid])+string("' left. ***\n"));
      delete[] FIFOtome;
      for(int i=1;i<31;i++)userpipes_used[i+31*userid] = false; //clear pipe to me
      exit(0);
    }else if(tokens[0]=="who")
    {
      cout<<"<ID> <nickname> <IP:port> <indicate me>"<<endl;
      for(int i=1;i<31;i++)
      {
        if(!userused[i])continue;
        cout<<i<<" "<<usernames[i]<<" "<< inet_ntoa(client_addrs[i]->sin_addr) << ":" << (int) ntohs(client_addrs[i]->sin_port);
        if(userid==i)cout<<"  "<<"<-me";
        cout<<endl;
      }
      continue;
    }else if(tokens[0]=="name")
    {
      if(tokens.size()==1)continue; //no enough argument

      bool c = false;
      //no same name
      for(int i=1;i<31;i++)
      {
        if(!userused[i])continue;
        if(string(usernames[i])==input.substr(input.find(tokens[1]),min(input.find("\r"),input.find("\n"))-input.find(tokens[1])))
        {
          cout<<"*** User '"<<input.substr(input.find(tokens[1]),min(input.find("\r"),input.find("\n"))-input.find(tokens[1]))<<"' already exists. ***"<<endl;
          c = true;
          break;
        }
      }
      if(c)continue;
      
      memcpy(usernames[userid],input.substr(input.find(tokens[1]),min(input.find("\r"),input.find("\n"))-input.find(tokens[1])).c_str(),strlen(input.substr(input.find(tokens[1]),min(input.find("\r"),input.find("\n"))-input.find(tokens[1])).c_str())+1);
      stringstream ss;
      ss<<"*** User from "<< inet_ntoa(client_addrs[userid]->sin_addr) << ":" << (int) ntohs(client_addrs[userid]->sin_port)<<" is named '"<<usernames[userid]<<"'. ***"<<endl;
      broadcast(ss.str());
      continue;
    }else if(tokens[0]=="tell")
    {
      if(tokens.size()<=2)continue; //no enough argument
      if(userused[stoi(tokens[1])]==0)
      {
        dprintf(STDOUT_FILENO,"*** Error: user #%s does not exist yet. ***\n",tokens[1].c_str());
        continue;
      }
      
      tell(stoi(tokens[1]),"*** "+string(usernames[userid])+" told you ***: "+input.substr(input.find(tokens[2]),min(input.find("\r"),input.find("\n"))-input.find(tokens[2]))+"\n");
      continue;
    }else if(tokens[0]=="yell")
    {
      if(tokens.size()<=1)continue; //no enough argument
      stringstream ss;
      ss<<"*** "<<usernames[userid]<<" yelled ***: "<<input.substr(input.find(tokens[1]),min(input.find("\r"),input.find("\n"))-input.find(tokens[1]))<<"\n";
      broadcast(ss.str());
      continue;
    }

    while(true)
    {
      // all commands between pipe
      if((it = find(it,tokens.end(),"|"))!=tokens.end())
      {
        // convert partial string vector into char**
        char **argv = new char* [distance(previt,it)+1];
        int i=0;
        for(auto theit=previt;theit!=it;theit++)
        {
          argv[i++] = (char *)(*theit).c_str();
        }
        argv[i] = NULL;
        
        // prepare pipe
        close(prevpipe[0]);
        close(prevpipe[1]);
        prevpipe[0] = thispipe[0];
        prevpipe[1] = thispipe[1];
        if (pipe(thispipe)){
          fprintf (stderr, "Pipe failed.\n");
          return EXIT_FAILURE;
        }
        // fork child and execute 
        pid_t pid = fork_unitil_success();

        if(pid == 0)
        {
          Process proc((char *)(*previt).c_str(),(char **)argv);
          close(prevpipe[1]);
          close(thispipe[0]);
          proc.input = prevpipe[0];
          proc.output = thispipe[1];
          proc.run();
        }else if(pid > 0)
        {
          childs.push_back(pid);
        }
        delete[] argv;
        it++;
        previt = it;
      }else  // the last command of a line
      {
        char **argv = new char* [distance(previt,it)+1];
        int i=0;
        for(auto theit=previt;theit!=it;theit++)
        {
          argv[i++] = (char *)(*theit).c_str();
        }
        argv[i] = NULL;
        
        close(prevpipe[0]);
        close(prevpipe[1]);
        prevpipe[0] = thispipe[0];
        prevpipe[1] = thispipe[1];

        auto dup_pipe = numberpipes.begin();
        bool new_pipe = false;
        bool new_user_pipe = false;
        int userpipe_to_user;
        // number pipe
        if((*--it)[0] == '|' || (*it)[0] == '!')
        {
          argv[i-1]=NULL;
          Numberpipe np;
          np.afterline = atoi( (*it).substr(1).c_str() );
          
          // merge pipe
          if((dup_pipe = find(numberpipes.begin(),numberpipes.end(),np.afterline))!=numberpipes.end())
          {
            thispipe[1] = dup_pipe->lastpipe[1];
            dup_pipe->proc_pids.insert(dup_pipe->proc_pids.end(),childs.begin(),childs.end());
          }else//new pipe
          {
            new_pipe = true;
            pipe(thispipe);
            np.lastpipe[0] = thispipe[0];
            np.lastpipe[1] = thispipe[1];
          }
          numberpipes.push_back(np);
        }

        //user pipe  >3434  != >
        if((*it)[0] == '>' && (*it).length() != 1)
        {
          argv[i-1]=NULL;
          userpipe_to_user = atoi( (*it).substr(1).c_str() );
          
          pipe(thispipe);
          // the user doesn't exist
          if(userpipe_to_user>30 || userpipe_to_user<=0 || !userused[userpipe_to_user])
          {
            printf("*** Error: user #%d does not exist yet. ***\n",userpipe_to_user);
            close(thispipe[1]);
            thispipe[1] = open("/dev/null", O_WRONLY);   // pipe to /dev/null
          }// pipe exist
          else if(userpipes_used[userid+31*userpipe_to_user])
          {
            printf("*** Error: the pipe #%d->#%d already exists. ***\n",userid,userpipe_to_user);
            close(thispipe[1]);
            thispipe[1] = open("/dev/null", O_WRONLY);
          }else//new user pipe
          {
            new_user_pipe = true;
            userpipes_used[userid+31*userpipe_to_user] = true;
            close(thispipe[1]);
            createNewFifo(("user_pipe/"+to_string(userid)+"to"+to_string(userpipe_to_user)).c_str());
            kill(userid_to_pid[userpipe_to_user],SIGUSR2);
            thispipe[1] = open(("user_pipe/"+to_string(userid)+"to"+to_string(userpipe_to_user)).c_str(),O_WRONLY);
            stringstream ss;
            ss<<"*** "<<usernames[userid]<<" (#"<<userid<<") just piped '"<<input.substr(0,min(input.find("\r"),input.find("\n")))<<"' to "<<usernames[userpipe_to_user]<<" (#"<<userpipe_to_user<<") ***\n";
            broadcast(ss.str());
          }
        }

        // fork child and execute 
        pid_t pid = fork_unitil_success();
        if(pid == 0)
        {
          Process proc((char *)(*previt).c_str(),(char **)argv);
          if((*it)[0] == '|' || (*it)[0] == '!' || ((*it)[0] == '>' && (*it).length() != 1))proc.output = thispipe[1];
          if((*it)[0] == '!')proc.err = thispipe[1];
          
          close(prevpipe[1]);
          proc.input = prevpipe[0];
          proc.run();
        }else if(pid > 0)
        {
          close(prevpipe[0]);
          close(prevpipe[1]);
          childs.push_back(pid);

          //if number pipe, no wait for processes, this code add wait list used when pipe to no number pipe
          if((*it)[0] == '|' || (*it)[0] == '!')
          {
            if(new_pipe)
            {
              numberpipes.back().proc_pids = childs;
            }else
            {
              dup_pipe->proc_pids.push_back(pid);
            }
            childs.clear();
          }

          // if userpipe
          if((*it)[0] == '>' && (*it).length() != 1)
          {
            close(thispipe[0]);
            close(thispipe[1]);
            childs.clear();
          }
        }
        

        //wait all process which needed
        for (auto p : childs)
        {
          waitpid(p,NULL,0);
        }
        delete[] argv;

        // next line
        break;
      }
    }
    
    

  }
  cerr<<"xxx";
  return 0;
}