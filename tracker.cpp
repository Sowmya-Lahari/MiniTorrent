#include <iostream>
#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <iomanip>
#define CHUNK_SZ 512000
#define MY_PORT 9000
using namespace std;

class tracker
{
    public:
    string id;
    bool status;
    string ip;
    int port;
};
class client
{
    public:
    int sock;
    string id;
    int port;
    bool active;
    string ip;
    string pwd;
};
class chunk_details
{
    public:
    string fname;
    int num;
    string sha;
    map <string,client *> *leechers;
};
class file
{
    public:
    string name;
    string owner;
    unsigned long long size;
    int state;
    int num_chunks;
    string sha;
    int cnt=0;
    map <string,chunk_details *> *chunks;
};
class group
{
    public:
    string id;
    string owner_id;
    map <string,client *> *members;
    map <string,file *> *files;
};
class client_groups
{
    public:
    string id;
    map <string,group *> *in_group;
};
class client_chunks
{
    public:
    string id;
    map <string,chunk_details*> *has_chunk;
};
class stop_share
{
    public:
    string gid;
    string id;
    string fname;
};
class details
{
    public:
    map <string,client *> *all_clients;
    map <string,group *> *all_groups;
    map <string,tracker *> *all_trackers;
    vector <class stop_share *> *all_stop_share;
} *cur_details;
map <string,vector<pair<string,string>>> requests;
void error(string s)
{
    cout<<errno<<":"<<s<<endl;
    exit(1);
}
void print_cp(int i)
{
    cout<<"checkpoint"<<i<<endl;
}
void init_cur_details()
{
    map<string, client*> *cur_cl=new map<string, client*>;
    cur_details->all_clients=cur_cl;
    map <string, class group*> *cur_gr=new map <string, class group*>;
    cur_details->all_groups=cur_gr;
    vector <class stop_share*> *ss=new vector <class stop_share*>;
    cur_details->all_stop_share=ss;
    //cur_details->requests=new map <string, vector<pair<string,string>>>;
}
void * handle_request(void *y)
{
    int client=*((int *)y);
    class client *cur_client=new class client();
    cur_client->sock=client;
    cur_client->ip="127.0.0.1";
    cur_client->id="-1";
    char* msg_rcvd=(char *)malloc(1024*sizeof(char));
    string reply_msg;
    int login=0;
    if(read(client,msg_rcvd,1024)<=0)
    {
        printf("Error while reading");
        return NULL;
    }
    reply_msg="Hiii form server";
    if(send(client,reply_msg.c_str(),reply_msg.size(),0)<0)
    {
        printf("Error while sending");
        return NULL;
    }
    while(1)
    {
        int bits_read=recv(client,msg_rcvd,5,0);
        int size=stoi(msg_rcvd);
        char *temp=(char *)malloc(size*sizeof(char));
        string msg="";
        int cur_sz=0;
        while(cur_sz<size)
        {
            bits_read=recv(client,temp,size,0);
            msg+=temp;
            cur_sz+=bits_read;
        }
        free(temp);
        msg=msg.substr(0,size);
        if (msg=="logout")
        {
            login=0;
            cur_details->all_clients->at(cur_client->id)->active=false;
            if(cur_details->all_clients!=NULL)
                cur_details->all_clients->at(cur_client->id)->active=false;
            reply_msg="Logged out";
        }
        else if(msg=="list_groups")
        {
            if(cur_details->all_groups->size()==0)
                reply_msg="No groups are present";
            else
            {
                string temp_data="";    
                map <string,class group*> grps=*cur_details->all_groups;
                for (auto x: grps)
                {
                    temp_data+=x.first+",";
                }
                temp_data.pop_back();
                reply_msg=temp_data;
            }
        }
        else
        {
            vector <string> comps;
            string temp="";
            int i=0;
            while(i<msg.size())
            {
                if(msg[i]==' ')
                {
                    comps.push_back(temp);
                    temp="";
                }
                else
                    temp+=msg[i];
                i++;
            }
            if(temp!="")
                comps.push_back(temp);
            if(comps[0]=="create_user")
            {
                string cid=comps[1];
                if(cur_details->all_clients->find(cid)==cur_details->all_clients->end())
                {
                    cur_client->id=cid;
                    cur_client->pwd=comps[2];
                    cur_client->ip=comps[3];
                    cur_client->port=stoi(comps[4]);
                    cur_client->active=false;
                    cur_details->all_clients->insert(make_pair(cid,cur_client));
                    reply_msg="User Created successfully";
                }
                else
                    reply_msg="User Already exists!";
            }
            else if(comps[0]=="login")
            {
                string cid=comps[1];
                if(cur_details->all_clients->find(comps[1])==cur_details->all_clients->end())
                    reply_msg="Invalid User";
                else 
                {
                    if(login==0)
                    {
                        if(cur_details->all_clients->at(comps[1])->pwd==comps[2])
                        {
                            reply_msg="Logged in successfully";
                            cur_details->all_clients->at(comps[1])->active=true;
                            cur_client->id=comps[1];
                            login=1;
                        }
                        else
                            reply_msg="Incorrect password";
                    }
                    else 
                    {
                        reply_msg="You have already logged in";
                    }
                }
            }
            else if(comps[0]=="create_group")
            {
                string gid=comps[1];
                if(login==1)
                {
                    if(cur_details->all_groups->find(gid)==cur_details->all_groups->end())
                    {
                        class group *new_group=new group();
                        new_group->id=gid;
                        new_group->owner_id=cur_client->id;
                        cur_details->all_groups->insert(make_pair(gid,new_group));
                        cur_details->all_groups->at(gid)->files=new map <string, class file*>;
                        cur_details->all_groups->at(gid)->members=new map <string, class client*>;
                        cur_details->all_groups->at(gid)->members->insert(make_pair(cur_client->id,cur_details->all_clients->at(cur_client->id)));
                        reply_msg="Group created successfully";
                    }
                    else
                        reply_msg="Group already exists";
                }
                else
                    reply_msg="Not logged in";
            }
            else if(comps[0]=="join_group")
            {
                if(login==1){
                string gid=comps[1];
                string cid=cur_client->id;
                if(cur_details->all_groups->find(gid)==cur_details->all_groups->end())
                {
                    reply_msg="Invalid group";
                }
                else
                {
                    string owner=cur_details->all_groups->at(gid)->owner_id;
                    reply_msg=owner;
                    class client owner_dets=*cur_details->all_clients->at(owner);
                    reply_msg+=','+owner_dets.ip+","+to_string(owner_dets.port);
                    if(requests.find(owner)==requests.end())
                    {
                        vector <pair<string,string>> *new_re=new vector<pair<string,string>>;
                        new_re->push_back(make_pair(cid,gid));
                        requests.insert(make_pair(owner,*new_re));
                    }
                    else
                        requests.at(owner).push_back(make_pair(cid,gid));
                    reply_msg="Request sent";
                }
                }
                else
                    reply_msg="Not logged in";
            }
            else if(comps[0]=="accept_request")
            {
                string gid=comps[1];
                string cid=comps[2];
                if(cur_details->all_groups->find(gid)==cur_details->all_groups->end())
                    reply_msg="Not a valid group id";
                else if(cur_details->all_groups->at(gid)->members->find(cid)==cur_details->all_groups->at(gid)->members->end())
                {
                    cur_details->all_groups->at(gid)->members->insert(make_pair(cid,cur_details->all_clients->at(cid)));
                    reply_msg="Joined group successfully";
                    auto req=requests.at(cur_client->id);
                    auto er=requests.at(cur_client->id).begin();
                    for(auto x=req.begin();x!=req.end();x++)
                    {
                        if(x->first==gid && x->second==cid)
                            er=x;
                    }
                    requests.at(cur_client->id).erase(er);
                    if(requests.at(cur_client->id).size()==0)
                        requests.erase(cur_client->id);
                }
                else
                    reply_msg="Already a group member";
            }
            else if(comps[0]=="leave_group")
            {
                string cid=cur_client->id;
                cur_details->all_groups->at(comps[1])->members->erase(cid);
                reply_msg="Removed from group";
            }
            else if(comps[0]=="list_requests")
            {
                //cout<<cur_client->id<<endl;
                if(cur_details->all_groups->find(comps[1])==cur_details->all_groups->end())
                    reply_msg="Invalid group";
                else if(cur_details->all_groups->at(comps[1])->owner_id!=cur_client->id)
                    reply_msg="You are not the owner";
                else if(requests.find(cur_client->id)==requests.end())
                    reply_msg="No requests are present";
                else
                {
                    string temp_data="";    
                    vector <pair<string,string>> req=requests.at(cur_client->id);
                    for (auto x: req)
                    {
                        if(x.second==comps[1])
                            temp_data+=x.second+","+x.first+":";
                    }
                    temp_data.pop_back();
                    reply_msg=temp_data;
                }
            }
            else if(comps[0]=="list_files")
            {   
                string gid=comps[1];
                if(cur_details->all_groups->find(gid)==cur_details->all_groups->end())
                {
                    reply_msg="Invalid group";
                }
                else
                {
                    if(cur_details->all_groups->at(gid)->files->size()==0)
                        reply_msg="No files are being shared";
                    else
                    {
                        reply_msg="";
                        map <string,class file*> cur_files=*cur_details->all_groups->at(gid)->files;
                        for(auto x:cur_files)
                            if(x.second->cnt>0)
                                reply_msg+=x.first+",";
                        reply_msg.pop_back();
                    }   
                }    
            }
            else if(comps[0]=="upload_file")
            {
                string fname=comps[1];
                string gid=comps[2];
                if(cur_details->all_groups->at(gid)->files->find(fname)==cur_details->all_groups->at(gid)->files->end())
                {
                    unsigned long long sz=stoull(comps[3]);
                    class file *new_file=new file();
                    new_file->name=fname;
                    new_file->size=sz;
                    new_file->owner=cur_client->id;
                    new_file->num_chunks=(sz/CHUNK_SZ)+((sz%CHUNK_SZ==0)?0:1);
                    new_file->state=false;
                    new_file->sha=comps[4];
                    new_file->cnt=1;
                    int dot=fname.find_last_of('.');
                    string new_fname=fname.substr(0,dot);
                    string ext=fname.substr(dot);
                    cur_details->all_groups->at(gid)->files->insert(make_pair(fname,new_file));
                    cur_details->all_groups->at(gid)->files->at(fname)->chunks=new map <string,class chunk_details *>;
                    for(int i=0;i<new_file->num_chunks;i++)
                    {
                        class chunk_details *new_chunk=new chunk_details();
                        new_chunk->leechers=new map<string,class client*>;
                        new_chunk->fname=fname;
                        new_chunk->num=i;
                        new_chunk->leechers->insert(make_pair(cur_client->id,cur_client));
                        new_chunk->sha=comps[5+i];
                        cur_details->all_groups->at(gid)->files->at(fname)->chunks->insert(make_pair(new_fname+"_"+to_string(i)+ext,new_chunk));
                    }
                    reply_msg="File uploaded successfully";
                }
                else
                    reply_msg="File already uploaded";

            }
            else if(comps[0]=="download_file")
            {
                if(cur_details->all_groups->find(comps[1])==cur_details->all_groups->end())
                    reply_msg="Invalid group";
                else if(cur_details->all_groups->at(comps[1])->members->find(cur_client->id)==cur_details->all_groups->at(comps[1])->members->end())
                    reply_msg="You are not a part of the group";
                else if(cur_details->all_groups->at(comps[1])->files->find(comps[2])==cur_details->all_groups->at(comps[1])->files->end())
                    reply_msg="File is not available";
                else    
                {
                    reply_msg="";
                    int pos=0;
                    pos=comps[2].find_last_of('.');
                    string name=comps[2].substr(0,pos);
                    string ext=comps[2].substr(pos+1);
                    for (pair<string,class chunk_details*> chunk: *(cur_details->all_groups->at(comps[1])->files->at(comps[2])->chunks))
                    {
                        string ch_name=chunk.first;
                        for(pair <string,class client *> leech: *(chunk.second->leechers))
                        {
                            if(leech.second->active)
                            {
                                reply_msg+=ch_name+","+leech.second->ip+","+to_string(leech.second->port)+","+chunk.second->sha+":";
                            }
                        }
                    }
                    reply_msg+=to_string(cur_details->all_groups->at(comps[1])->files->at(comps[2])->size);
                }
                //cout<<reply_msg<<endl;
            }
            else if(comps[0]=="has_chunk")
            {
                cur_details->all_groups->at(comps[1])->files->at(comps[2])->chunks->at(comps[3])->leechers->insert(make_pair(cur_client->id,cur_details->all_clients->at(cur_client->id)));
                reply_msg="Chunk added";
                if(comps[3].substr(comps[3].find_last_of('_')+1,1)=="0")
                    cur_details->all_groups->at(comps[1])->files->at(comps[2])->cnt++;
            }
            else if(comps[0]=="stop_share")
            {
                if(cur_details->all_groups->find(comps[1])==cur_details->all_groups->end())
                    reply_msg="Invalid group";
                else if(cur_details->all_groups->at(comps[1])->members->find(cur_client->id)==cur_details->all_groups->at(comps[1])->members->end())
                    reply_msg="You are not a part of the group";
                else if(cur_details->all_groups->at(comps[1])->files->find(comps[2])==cur_details->all_groups->at(comps[1])->files->end())
                    reply_msg="File is not available";
                else
                {
                    class stop_share *new_ss=new stop_share();
                    new_ss->fname=comps[2];
                    new_ss->gid=comps[1];
                    new_ss->id=cur_client->id;
                    bool flag=false;
                    for (class stop_share *ss: *(cur_details->all_stop_share))
                    {
                        if (ss->fname==new_ss->fname && ss->gid==new_ss->gid && ss->id==new_ss->id)
                            flag=true;
                    }
                    if(flag)
                        reply_msg="Already stopped sharing";
                    else
                    {
                        reply_msg="Stopped sharing";
                        cur_details->all_stop_share->push_back(new_ss);
                        cur_details->all_groups->at(comps[1])->files->at(comps[2])->cnt--;
                    }
                }
            }
            else if(comps[0]=="change_owner")
            {
                auto new_owner=cur_details->all_groups->at(comps[1])->members->rbegin();
                if(new_owner->first==cur_details->all_groups->at(comps[1])->owner_id)
                {
                    reply_msg="No other members are there in the group. Group is deleted";
                    cur_details->all_groups->erase(comps[1]);
                }
                else
                {
                    cur_details->all_groups->at(comps[1])->owner_id=new_owner->first;
                    reply_msg="Changed the owner of the group and you are removed from the group";
                    cur_details->all_groups->at(comps[1])->members->erase(cur_client->id);
                }
            }
            else
                reply_msg="Invalid request";
        }
        string sz=to_string(reply_msg.size());
        int len=sz.size();
        int diff=5-len;
        while(diff--)
            sz="0"+sz;
        if(send(client,sz.c_str(),5,0)<0)
        {
            printf("Error while sending");
            return NULL;
        }
        if(send(client,reply_msg.c_str(),reply_msg.size(),0)<0)
        {
            printf("Error while sending");
            return NULL;
        }
    }
    
}

void * listen_requests(void *y)
{
    class tracker *my_tracker=(tracker *)y;
    int my_sock=0;
    //Create a socket
    if((my_sock=socket(AF_INET,SOCK_STREAM,0))<=0)
        error("Error while creating a socket");

    //Set socket options
    int x=1;
    if(setsockopt(my_sock,SOL_SOCKET,SO_REUSEADDR|SO_REUSEPORT,&x,sizeof(x)))
        error("Error while setting options");
    
    //Bind socket
    class sockaddr_in my_add;
    my_add.sin_port=htons(my_tracker->port);
    my_add.sin_family=AF_INET;
    if(inet_pton(AF_INET,my_tracker->ip.c_str(),&my_add.sin_addr)<=0)
        error("Invalid address");
    int sz=sizeof(my_add);
    if(bind(my_sock,(const sockaddr *)&my_add,(socklen_t)sz)<0)
        error("Error while binding");
    while(1)
    {
        //Listen
        if(listen(my_sock,1)<0)
            error("Error while listening");

        //Accept
        int *client=(int *)malloc(sizeof(int));
        if((*client=accept(my_sock,(sockaddr*)&my_add,(socklen_t*)&sz))<=0)
                error("Error while accepting");
        else
        {
            pthread_t th1;
            int tr1=pthread_create(&th1,NULL,handle_request,(void *)client);
        }
    }
}

int main(int argc,char **argv)
{
    if(argc!=3)
    {
        cout<<"Invalid number of arguments";
        return 0;
    }
    string fname=argv[1];
    string id=argv[2];
    FILE* fp = fopen(fname.c_str(), "r");
    if (fp == NULL)
        error("Not able to open the file "+fname);
    char* line = NULL;
    size_t len = 0;
    map <string,class tracker*> tracker_details;
    while((getline(&line,&len,fp))!=-1) 
    {
        string temp=line,field="";
        int i=0;
        vector <string> x;
        while(i<temp.size())
        {
            if(temp[i]==',')
            {
                x.push_back(field);
                field="";
            }
            else
                field+=temp[i];
            i++;
        }
        if(field!="")
            x.push_back(field);
        class tracker *track=new tracker();
        track->id=x[0];
        track->ip=x[1];
        track->port=stoi(x[2]);
        tracker_details[track->id]=track;
    }
    fclose(fp);
    class tracker *my_tracker=new tracker();
    my_tracker=tracker_details[id];
    cur_details=new details;
    cur_details->all_trackers=&tracker_details;
    init_cur_details();
    pthread_t thread1;
    pthread_attr_t ar1;
    pthread_attr_init(&ar1);
    pthread_attr_setstacksize(&ar1,PTHREAD_STACK_MIN+3145728);
    int tr1=pthread_create(&thread1,&ar1,listen_requests,(void *)my_tracker);
    while(1)
    {
        string abort="quit";
        string cmd;
        cin>>cmd;
        if(cmd==abort)
        {
            pthread_join(tr1,NULL);
            break;
        }
    }
    return 0;
}