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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <sstream>
#include <sys/stat.h>    
#include <fcntl.h>

using namespace std;

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

int NPshell::myprintenv(string s){
  for(string enval : myenvp)
  {
    if(enval.find(s+"=",0) == 0)
    {
      dprintf(mysocket,"%s",(enval.substr(s.length()+1)+"\n").c_str());
      return 1;
    }
  }
  return 0;
}

int NPshell::mysetenv(string var,string value){
  for(auto& s : myenvp)
  {
    if(s.find(var+"=",0) == 0)
    {
      s=var+"="+value;
      return 1;
    }
  }
  myenvp.push_back(var+"="+value);
  return 0;
}

int fork_unitil_success(){
  int r;
  while( (r=fork())==-1 );
  return r;
}

void NPshell::broadcast(string s){
  for (int fd=0; fd<nfds; ++fd)
	{
    if (fd != msock && FD_ISSET(fd, afds))
		{
			dprintf(fd,"%s",s.c_str());
		}
	}
}

NPshell::NPshell(int sock,char** envp,sockaddr_in fsin,fd_set *afds,int msock,int userid,int userused[31],NPshell** get_user_by_id){
  for (char **env = envp; *env != 0; env++)
  {
    myenvp.push_back(string(*env));   
  }
  this->get_user_by_id = get_user_by_id;
  this->userid = userid;
  this->userused = userused;
  mysetenv("PATH","bin:.");
  signal(SIGCHLD,SIG_IGN);
  mysocket = sock;
  client_addr = fsin;
  nfds = getdtablesize();
  this->afds = afds;
  this->msock = msock;
  dprintf(mysocket,"%s","****************************************\n** Welcome to the information server. **\n****************************************\n");
  stringstream ss;
  name = "(no name)";
  ss << "*** User '"<<name<<"' entered from " << inet_ntoa(client_addr.sin_addr) << ":" << (int) ntohs(client_addr.sin_port) << ". ***\n";
  broadcast(ss.str());
  dprintf(mysocket,"%% ");
}


NPshell::NPshell(){}

int NPshell::runline(string line){
  input = line;
  vector<string> tokens = split(input);
  if(tokens.size() == 0)
  {
    dprintf(mysocket,"%% ");
    return 0;  // continue when empty line
  }
  int prevpipe[2]={-1,-1};
  int thispipe[2]={-1,-1};
  
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
      dprintf(mysocket,"*** Error: user #%d does not exist yet. ***\n",user_pipe_from);
      thispipe[0]=open("/dev/null", O_RDONLY);;
    }else if(!userpipes_to_me[user_pipe_from].used)
    {
      dprintf(mysocket,"*** Error: the pipe #%d->#%d does not exist yet. ***\n",user_pipe_from,userid);
      thispipe[0]=open("/dev/null", O_RDONLY);;
    }else
    {
      userpipes_to_me[user_pipe_from].used = false;
      close(userpipes_to_me[user_pipe_from].thepipe[1]);
      thispipe[0] = userpipes_to_me[user_pipe_from].thepipe[0];
      childs = userpipes_to_me[user_pipe_from].proc_pids;
      stringstream ss;
      ss<<"*** "<<name<<" (#"<<userid<<") just received from "<<get_user_by_id[user_pipe_from]->name<<" (#"<<user_pipe_from<<") by '"<<line.substr(0,min(line.find("\r"),line.find("\n")))<<"' ***\n";
      broadcast(ss.str());
    }
  }
    
  // built-in commands
  if(tokens[0] == "printenv")
  {
    if(tokens.size()==1)return -1; //no environment var
    myprintenv(tokens[1]);
    dprintf(mysocket,"%% ");
    return 0;

  }else if(tokens[0]=="setenv")
  {
    if(tokens.size()<=2)return -1; //no enough argument
    mysetenv(tokens[1],tokens[2]);
    dprintf(mysocket,"%% ");
    return 0;
  }else if(tokens[0]=="exit")
  {
    for(auto &p : userpipes_to_me)
    {
      p.used=false;
      close(p.thepipe[0]);
      close(p.thepipe[1]);
    }
    return 2;
  }else if(tokens[0]=="who")
  {
    stringstream ss;
    ss<<"<ID> <nickname> <IP:port> <indicate me>"<<endl;
    for(int i=1;i<31;i++)
    {
      if(!userused[i])continue;
      ss<<i<<" "<<get_user_by_id[i]->name<<" "<< inet_ntoa(get_user_by_id[i]->client_addr.sin_addr) << ":" << (int) ntohs(get_user_by_id[i]->client_addr.sin_port);
      if(userid==i)ss<<"  "<<"<-me";
      ss<<endl;
    }
    ss<<"% ";
    dprintf(mysocket,"%s",ss.str().c_str());
    return 0;
  }else if(tokens[0]=="name")
  {
    if(tokens.size()==1)return -1; //no enough argument

    //no same name
    for(int i=1;i<31;i++)
    {
      if(!userused[i])continue;
      if(get_user_by_id[i]->name==line.substr(line.find(tokens[1]),min(line.find("\r"),line.find("\n"))-line.find(tokens[1])))
      {
        stringstream ss;
        ss<<"*** User '"<<line.substr(line.find(tokens[1]),min(line.find("\r"),line.find("\n"))-line.find(tokens[1]))<<"' already exists. ***"<<endl;
        ss<<"% ";
        dprintf(mysocket,"%s",ss.str().c_str());
        return 0;
      }
    }
    
    name = line.substr(line.find(tokens[1]),min(line.find("\r"),line.find("\n"))-line.find(tokens[1]));
    stringstream ss;
    ss<<"*** User from "<< inet_ntoa(client_addr.sin_addr) << ":" << (int) ntohs(client_addr.sin_port)<<" is named '"<<name<<"'. ***"<<endl;
    broadcast(ss.str());
    dprintf(mysocket,"%% ");
    return 0;
  }else if(tokens[0]=="tell")
  {
    if(tokens.size()<=2)return -1; //no enough argument
    if(userused[stoi(tokens[1])]==0)
    {
      dprintf(mysocket,"*** Error: user #%s does not exist yet. ***\n",tokens[1].c_str());
      dprintf(mysocket,"%% ");
      return 0;
    }
    
    dprintf(get_user_by_id[stoi(tokens[1])]->mysocket,"*** %s told you ***: %s\n",name.c_str(),line.substr(line.find(tokens[2]),min(line.find("\r"),line.find("\n"))-line.find(tokens[2])).c_str());
    dprintf(mysocket,"%% ");
    return 0;
  }else if(tokens[0]=="yell")
  {
    if(tokens.size()<=1)return -1; //no enough argument
    stringstream ss;
    ss<<"*** "<<name<<" yelled ***: "<<line.substr(line.find(tokens[1]),min(line.find("\r"),line.find("\n"))-line.find(tokens[1]))<<"\n";
    broadcast(ss.str());
    dprintf(mysocket,"%% ");
    return 0;
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
        Process proc((char *)(*previt).c_str(),(char **)argv,myenvp,mysocket);
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
          dprintf(mysocket,"*** Error: user #%d does not exist yet. ***\n",userpipe_to_user);
          close(thispipe[1]);
          thispipe[1] = open("/dev/null", O_WRONLY);   // pipe to /dev/null
        }// pipe exist
        else if((get_user_by_id[userpipe_to_user]->userpipes_to_me)[userid].used)
        {
          dprintf(mysocket,"*** Error: the pipe #%d->#%d already exists. ***\n",userid,userpipe_to_user);
          close(thispipe[1]);
          thispipe[1] = open("/dev/null", O_WRONLY);
        }else//new user pipe
        {
          new_user_pipe = true;
          (get_user_by_id[userpipe_to_user]->userpipes_to_me)[userid].thepipe[0] = thispipe[0];
          (get_user_by_id[userpipe_to_user]->userpipes_to_me)[userid].thepipe[1] = thispipe[1];
          (get_user_by_id[userpipe_to_user]->userpipes_to_me)[userid].used = true;
          (get_user_by_id[userpipe_to_user]->userpipes_to_me)[userid].from_user_id = userid;
          (get_user_by_id[userpipe_to_user]->userpipes_to_me)[userid].to_user_id = userpipe_to_user;
          stringstream ss;
          ss<<"*** "<<name<<" (#"<<userid<<") just piped '"<<line.substr(0,min(line.find("\r"),line.find("\n")))<<"' to "<<get_user_by_id[userpipe_to_user]->name<<" (#"<<userpipe_to_user<<") ***\n";
          broadcast(ss.str());
        }
      }

      // fork child and execute 
      pid_t pid = fork_unitil_success();
      if(pid == 0)
      {
        Process proc((char *)(*previt).c_str(),(char **)argv,myenvp,mysocket);
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

        // if new userpipe
        if(new_user_pipe)
        {
          (get_user_by_id[userpipe_to_user]->userpipes_to_me)[userid].proc_pids = childs;
          childs.clear();
        }

        // if userpipe to dev/null
        if((*it)[0] == '>' && (*it).length() != 1 && !new_user_pipe)
        {
          close(thispipe[0]);
          close(thispipe[1]);
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
  dprintf(mysocket,"%% ");
  return 0;
}