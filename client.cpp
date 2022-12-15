#include <iostream>
#include <unistd.h> 
#include <stdio.h> 
#include <pwd.h>
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <signal.h>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <iomanip>
#define CHUNK_SZ 512000
#define MY_PORT 9000
using namespace std;
void terminate(int);
long long get_filesize(string);
string cur_sha;
class tracker
{
    public:
    string id;
    bool status;
    string ip;
    int port;
};

class downloads
{
    public:
    string gid;
    string fname;
    int state;
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
    vector <string> *groups;
    vector <string> *own_groups;
}*my_details;

class details
{
    public:
    map <string,tracker *> *all_trackers;
    vector <class downloads*> *downloads;
} *cur_details;
string get_path(string);

void error(string s)
{
    cout<<errno<<":"<<s<<endl;
    exit(1);
}

bool compare(string f1,string f2)
{
    int f1_ind=f1.find_last_of('.');
    int f2_ind=f2.find_last_of('.');
    f1=f1.substr(0,f1_ind);
    f2=f2.substr(0,f2_ind);
    f1_ind=f1.find_last_of('_');
    f2_ind=f2.find_last_of('_');
    f1=f1.substr(f1_ind+1);
    f2=f2.substr(f2_ind+1);
    //cout<<f1<<"  "<<f2<<endl;
    if(stoi(f1)<stoi(f2))
        return true;
    return false;
}

string assemble_file(string fname,string path)
{
    //cout<<fname<<" "<<path<<endl;
    SHA_CTX full_hash;
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1_Init(&full_hash);
    vector <string> chunks;
    DIR *dptr;
	struct dirent *cur=(struct dirent *)malloc(sizeof(struct dirent));
    struct stat *file=(struct stat *)malloc(sizeof(struct stat));
	//Opened the directory
	dptr=opendir(".");
    int dot=fname.find_last_of('.');
    string new_fname=fname.substr(0,dot)+"_";
    //cout<<new_fname<<endl;
    string ext=fname.substr(dot);
	if(dptr!=NULL)
	{
        //cout<<"here"<<endl;
		//Reading the content of the directory
		while((cur=readdir(dptr))!=NULL)
		{	
			string temp=cur->d_name;
            //cout<<temp.substr(0,new_fname.size())<<" "<<new_fname<<" "<<temp<<endl;
            if (temp.substr(0,new_fname.size())==new_fname)
                chunks.push_back(temp);
        }
    }
    else
        error("Error while opening directory");
    //cout<<chunks.size();
    sort(chunks.begin(),chunks.end(),compare);
    int ffd=0;
    path=get_path(path);
    //cout<<path+"/"+fname<<endl;
    if((ffd=open((path+"/"+fname).c_str(),O_TRUNC |O_WRONLY|O_APPEND|O_CREAT,00777))<0)
    {
        cout<<"Invalid path";
        return " ";
    }
    //cout<<chunks.size();
    string cur_sha="";
    for(string name : chunks)
    {
        int chunk_size=512000;
        int nfd=0;
        //cout<<name<<endl;
        if((nfd=open(name.c_str(),O_RDONLY))<0)
            error("Error while opening file");
        char buf[chunk_size];
        int bits_read=0;
        bits_read=read(nfd,buf,chunk_size);
        //cout<<bits_read<<endl;
        if(bits_read>0)
        {
            if(write(ffd,buf,bits_read)<0)
                error("Error while writing to the file");
            SHA1_Update(&full_hash,buf,bits_read);
            unsigned char hash2[SHA_DIGEST_LENGTH];
            unsigned char* data=(unsigned char*)buf;
            SHA1(data,bits_read,hash2);    
            char tmphash[SHA_DIGEST_LENGTH];
            int i=0;
            stringstream buffer;
            for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
                buffer<<hex<<setfill('0')<<setw(2)<<static_cast<unsigned>(hash2[i]);
            string hexString = buffer.str();
            //cout<<hexString<<endl;
            cur_sha+=hexString.substr(0,20);
        }
        else if(bits_read<0)
            error("104Error while reading");
        else
            continue;
    }
    SHA1_Final(hash,&full_hash);
    //for(int i=0;i<SHA_DIGEST_LENGTH;i++)
    //    printf("%02x",hash[i]);
    //cout<<" full hash"<<endl;
    return cur_sha;
}

string get_path(string name)
{
    char cur_dr[2048];
    string usr_home;
    getcwd(cur_dr,1000);
    struct passwd *cu = getpwuid(getuid());
	usr_home=cu->pw_dir;
	string new_curdir="";
    if(name[0]=='/')	//If the path starts with / append / to result
        new_curdir="/";
	else if(name[0]=='~') //If the path starts with ~ append home directory path to result
	{
		name=name.substr(1);
		if(name.size()>0)
			name=name.substr(1);
		new_curdir=usr_home;
		if(name.size()==0) 	//If the path is just ~ then return result
			return new_curdir;
	}
	else if(name[0]!='.')	//If the path starts with anything other than /,~,. then append current directory
	{
		new_curdir=cur_dr;
	} 
	char *sparts=strtok((char *)name.c_str(),"/"); //Split the path to tokens seperated by /
	while(sparts!=NULL)
	{	
		string part=sparts;
		if(part[0]=='.' && part[1]=='.') //If the current token is .. then extract parent dir of result
		{
			if(new_curdir=="")
			{
				new_curdir=cur_dr;
			}
			new_curdir=new_curdir.substr(0,new_curdir.find_last_of("/"));
            if(new_curdir.size()==0)
                new_curdir="/";
		}
		else if(part[0]=='.') //If . is at the start assign cur dir to result else ignore
		{
			if(new_curdir=="")
				new_curdir=cur_dr;
		}
		else //If the token starts with a character append the token to result
		{
            if(new_curdir=="" || new_curdir=="/")
                new_curdir+=part;
            else
			    new_curdir=new_curdir+"/"+part;
		}
        sparts=strtok(NULL,"/"); //Get the next token
	}
    //cout<<new_curdir<<endl;
	return new_curdir;
}

void *handle_request(void *msg)
{
    int client=*((int *)msg);
    char* msg_rcvd=(char *)malloc(1024*sizeof(char));
    string reply_msg;
    if(read(client,msg_rcvd,512)<=0)
    {
        printf("Error while reading");
        return NULL;
    }
    string fname=msg_rcvd;
    int ffd=0;
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA_CTX inter;
    SHA1_Init(&inter);
    long long fsz=get_filesize(fname);
    if((ffd=open(fname.c_str(),O_RDONLY))<0)
        error("Error while reading file46");
    char msg_sent[512000];
    int total=0;
    while(total<fsz)
    {
        int read_bytes=0;
        string blank="";
        read_bytes=read(ffd,msg_sent,fsz);
        if(read_bytes>0)
        {
            if(send(client,msg_sent,read_bytes,0)<0)
                error("Error while sending");
            SHA1_Update(&inter,msg_sent,read_bytes);
            if(send(client,"",0,0)<0)
                error("Error while sending");
        }
        SHA1_Final(hash,&inter);
        if(read_bytes<0)
            error("Error while reading the file");
        total+=read_bytes;
    }
    return NULL;
}

void *make_server(void * msg)
{
    char *temp=(char *)msg;
    string ip_port=temp;
    int pos=ip_port.find_first_of(',');
    string ip=ip_port.substr(0,pos);
    int port=stoi(ip_port.substr(pos+1));
    //Create a socket
    int my_sock=0;
    if((my_sock=socket(AF_INET,SOCK_STREAM,0))<=0)
        error("Error while creating a socket");

    //Set socket options
    int x=1;
    if(setsockopt(my_sock,SOL_SOCKET,SO_REUSEADDR|SO_REUSEPORT,&x,sizeof(x)))
        error("Error while setting options");
    
    //Bind socket
    class sockaddr_in my_add;
    my_add.sin_port=htons(port);
    my_add.sin_family=AF_INET;

    if(inet_pton(AF_INET,ip.c_str(),&my_add.sin_addr)<=0)
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
void terminate(int signum)
{
    pthread_exit(0);
}

void print_cp(int i)
{
    cout<<"checkpoint"<<i<<endl;
}
long long get_filesize(string filename)
{
    struct stat *file=(struct stat *)malloc(sizeof(struct stat));
    stat(filename.c_str(),file);
    return file->st_size;
}

vector <string> break_file(string fname)
{
    //cout<<fname<<"321"<<endl;
    vector <string> res;
    SHA_CTX full_hash;
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1_Init(&full_hash);
    unsigned long long fsize=get_filesize(fname);
    fname=get_path(fname);
    int slash=fname.find_last_of('/');
    if(slash<0)
        slash=-1;
    string new_fname=fname.substr(slash+1);
    new_fname=new_fname.substr(0,new_fname.find_last_of('.'));
    string ext=fname.substr(fname.find_last_of('.'));
    //cout<<new_fname<<"  333   "<<ext<<endl;
    int i=0;
    int fd=0;
    if((fd=open(fname.c_str(),O_RDONLY))<0)
        error("Error while reading file");
    char buf[512000];
    string blank="";
    int read_bytes=0;
    int c=0;
    while((read_bytes=read(fd,buf,sizeof(buf))))
    {
        c++;
        string cur_name=new_fname+"_"+to_string(i++)+ext;
        //cout<<cur_name<<endl;
        int new_fd=0;
        if((new_fd=open(cur_name.c_str(),O_TRUNC|O_WRONLY|O_CREAT,00777))<0)
            error("Error while creating new file");
        if(write(new_fd,buf,read_bytes)<0)
            error("Error while writing to new file");
        unsigned char hash2[SHA_DIGEST_LENGTH];
        unsigned char* data=(unsigned char*)buf;
        SHA1(data,read_bytes,hash2);    
        char tmphash[SHA_DIGEST_LENGTH];
        int i=0;
        stringstream buffer;
        for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
            buffer<<hex<<setfill('0')<<setw(2)<<static_cast<unsigned>(hash2[i]);
        string hexString = buffer.str();
        res.push_back(hexString);
        SHA1_Update(&full_hash,buf,read_bytes);
    }
    SHA1_Final(hash,&full_hash);
    stringstream buffer;
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
        buffer<<hex<<setfill('0')<<setw(2)<<static_cast<unsigned>(hash[i]);
    string hexString=buffer.str();
    res.insert(res.begin(),hexString);
    res.insert(res.begin(),to_string(fsize));
    return res;
}
string send_receive(string msg,int my_sock,int &size)
{
    char msg_rcvd[1024];
    string sz=to_string(msg.size());
    int len=sz.size();
    int diff=5-len;
    while(diff--)
        sz="0"+sz;
    if(send(my_sock,sz.c_str(),5,0)<0)
    {
        printf("Error while sending");
        return NULL;
    }
    if(send(my_sock,msg.c_str(),msg.size(),0)<0)
    {
        printf("Error while sending");
        return NULL;
    }
    int bits_read=recv(my_sock,msg_rcvd,5,0);
    size=stoi(msg_rcvd);
    char *temp=(char *)malloc(size*sizeof(char));
    string res;
    int cur_sz=0;
    while(cur_sz<size)
    {
        bits_read=recv(my_sock,temp,size,0);
        res+=temp;
        cur_sz+=bits_read;
    }
    free(temp);
    res=res.substr(0,size);
    return res;
}
void * receive_chunk(void *det)
{
    char *ch_details=(char *)det;
    string msg=ch_details;
    vector <string> comps;
    string temp="";
    int i=0;
    while(i<msg.size())
    {
        if(msg[i]==',')
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
    string chunk_name=comps[0];
    long long ch_size=stoll(comps[1]);
    int port=stoi(comps[2]);
    string ip=comps[3];
    int my_sock=0;
    //Create a socket
    if((my_sock=socket(AF_INET,SOCK_STREAM,0))<0)
        error("Error while creating a socket");

    
    //Connect to server
    class sockaddr_in my_add;
    my_add.sin_family=AF_INET;
    my_add.sin_port=htons(port);
    if(inet_pton(AF_INET,ip.c_str(),&my_add.sin_addr)<=0)
        error("191Invalid address");
    int sz=sizeof(my_add);
    if(connect(my_sock,(sockaddr*)&my_add,sz)<0)
        error("Error while connecting");
    char *msg_temp=(char *)malloc(1024*sizeof(char));
    strcpy(msg_temp,chunk_name.c_str());
    char msg_rcvd[ch_size]={0};
    if(send(my_sock,chunk_name.c_str(),chunk_name.size(),0)<0)
            error("Error while sending");
    int ffd=0;
    if((ffd=open(chunk_name.c_str(),O_TRUNC |O_WRONLY|O_APPEND|O_CREAT,00777))<0)
        error("Error while opening file");
    int bits_read=0;
    int total_sz=0;
    while(total_sz<ch_size)
    {
        bits_read=recv(my_sock,msg_rcvd,ch_size,0);
        if(bits_read>0)
        {
            if(write(ffd,msg_rcvd,bits_read)<0)
                error("Error while writing to the file");
        }
        else if(bits_read<0)
            error("Error while reading");
        else
            break;
        total_sz+=bits_read;
    }
    return NULL;
}

int main(int argc, char **argv)
{
    if(argc!=3)
    {
        cout<<"Invalid number of arguments";
        return 0;
    }
    vector <string> own_groups;
    vector <string> my_groups;
    string fname=argv[2];
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
    free(line);
    cur_details=new details();
    cur_details->all_trackers=&tracker_details;
    cur_details->downloads=new vector<downloads*>;
    my_details=new client();
    string ip_port=argv[1];
    int pos=ip_port.find_first_of(':');
    string ip=ip_port.substr(0,pos);
    int port=stoi(ip_port.substr(pos+1));
    signal(SIGSTOP,terminate);
    pthread_t m_server;
    pthread_attr_t ar1;
    pthread_attr_init(&ar1);
    pthread_attr_setstacksize(&ar1,PTHREAD_STACK_MIN+3145728);
    string temp_msg=ip+","+to_string(port);
    int tr1=pthread_create(&m_server,&ar1,make_server,(void *)temp_msg.c_str());
    
    int my_sock=0;
    //Create a socket
    if((my_sock=socket(AF_INET,SOCK_STREAM,0))<0)
        error("Error while creating a socket");

    
    //Connect to server
    auto t1=*tracker_details.begin();
    class sockaddr_in my_add;
    my_add.sin_family=AF_INET;
    my_add.sin_port=htons(t1.second->port);
    if(inet_pton(AF_INET,t1.second->ip.c_str(),&my_add.sin_addr)<=0)
        error("191Invalid address");
    int sz=sizeof(my_add);
    if(connect(my_sock,(sockaddr*)&my_add,sz)<0)
        error("Error while connecting");

    char *msg_temp=(char *)malloc(1024*sizeof(char));
    strcpy(msg_temp,"Hello from the client side");
    char msg_rcvd[1024]={0};
    if(send(my_sock,msg_temp,1024,0)<0)
            error("Error while sending");
    int bits_read=read(my_sock,msg_rcvd,sizeof(msg_rcvd));
    if(bits_read<0)
        error("Error while reading");
    string msg;
    map <string,class details> history; 
    map <string,class client> client_history;
    while(1)
    {
        getline(cin,msg);
        if (msg=="logout")
        {
            int size=0;
            string lists=send_receive(msg,my_sock,size);
            if(history.find(my_details->id)==history.end())
                history[my_details->id]=*cur_details;
            if(client_history.find(my_details->id)==client_history.end())
                client_history[my_details->id]=*my_details;
            my_groups.clear();
            own_groups.clear();
            cout<<"logged out!"<<endl;
        }
        else if(msg=="list_groups")
        {
            int size=0;
            string lists=send_receive(msg,my_sock,size);
            if(lists=="No groups are present")
                cout<<"No groups are present"<<endl;
            else
            {
                string temp="";
                int i=0;
                cout<<"The available groups are"<<endl;
                while(i<lists.size())
                {
                    if(lists[i]==',')
                    {
                       cout<<temp<<endl;
                        temp="";
                    }
                    else
                        temp+=lists[i];
                    i++;
                }
                if(temp!="")
                    cout<<temp<<endl;
            }
        }
        else if(msg=="show_downloads")
        {
            if(cur_details->downloads==NULL)
                cout<<"No Downloads"<<endl;
            else
            {
                map <int,string> state;
                state[0]="D";
                state[1]="C";
                for(class downloads *d: *cur_details->downloads)
                    cout<<"["<<state[d->state]<<"] ["<<d->gid<<"] "<<d->fname<<endl;
                cout<<"D(Downloading), C(Complete)"<<endl;
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
            if(comps[0]=="create_user" && comps.size()==3)
            {
                msg+=" "+ip+" "+to_string(port);
                int size=0;
                string lists=send_receive(msg,my_sock,size);
                lists=lists.substr(0,size);
                cout<<lists<<endl;
            }
            else if(comps[0]=="login" && comps.size()==3)
            {
                if(client_history.find(comps[1])!=client_history.end())
                {
                    my_details=&client_history[comps[1]];
                    if(my_details->groups->size()>0)
                        my_groups=*my_details->groups;
                    if(my_details->own_groups->size()>0)
                        own_groups=*my_details->own_groups;
                }
                else
                {
                    my_details->id=comps[1];
                    my_details->pwd=comps[2];
                    my_details->ip=ip;
                    my_details->port=port;
                    my_details->groups=new vector<string>;
                    my_details->own_groups=new vector<string>;
                }
                my_details->active=true;
                if(history.find(comps[1])!=history.end())
                    cur_details=&history[comps[1]];
                int size=0;
                string lists=send_receive(msg,my_sock,size);
                lists=lists.substr(0,size);
                cout<<lists<<endl;
            }
            else if(comps[0]=="create_group" && comps.size()==2)
            {
                int size=0;
                string lists=send_receive(msg,my_sock,size);
                lists=lists.substr(0,size);
                cout<<lists<<endl;
                if(lists=="Group created successfully")
                {
                    own_groups.push_back(comps[1]);
                }
                my_groups.push_back(comps[1]);
                my_details->groups=&my_groups;
                my_details->own_groups=&own_groups;
            }
            else if(comps[0]=="join_group" && comps.size()==2)
            {
                bool owner=false;
                for (auto x:my_groups)
                {
                    if (x==comps[1])
                        owner=true;
                }
                if(owner==true)
                    cout<<"You are the owner of the group "<<comps[1]<<endl;
                else
                {
                    int size=0;
                    string lists=send_receive(msg,my_sock,size);
                    lists=lists.substr(0,size);
                    cout<<lists<<endl;
                    my_groups.push_back(comps[1]);
                    my_details->groups=&my_groups;
                }
            }
            else if(comps[0]=="leave_group" && comps.size()==2)
            {
                bool owner=false;
                auto iter=own_groups.begin();
                for(auto x=own_groups.begin();x!=own_groups.end();x++)
                {
                    if (*x==comps[1])
                    {
                        iter=x;
                        owner=true;
                    }
                }
                if(owner==true)
                {
                    msg="change_owner "+comps[1];
                    int size=0;
                    string lists=send_receive(msg,my_sock,size);
                    lists=lists.substr(0,size);
                    cout<<lists<<endl;
                    own_groups.erase(iter);
                }
                else
                {
                    bool present=false;
                    auto iter_y=my_groups.begin();
                    for(auto x=my_groups.begin();x!=my_groups.end();x++)
                    {
                        cout<<*x<<endl<<comps[1]<<endl;
                        if (*x==comps[1])
                        {
                            iter_y=x;
                            present=true;
                        }
                    }
                    if(present==true)
                    {
                        int size=0;
                        string lists=send_receive(msg,my_sock,size);
                        lists=lists.substr(0,size);
                        cout<<lists<<endl;
                        my_groups.erase(iter_y);
                        my_details->groups=&my_groups;
                    }
                    else
                    {
                        cout<<"You are not a member of the group"<<endl;
                    }
                }
            }
            else if(comps[0]=="list_requests" && comps.size()==2)
            {
                int size=0;
                string lists=send_receive(msg,my_sock,size);
                lists=lists.substr(0,size);
                if(lists=="No requests are present")
                    cout<<"No requests are present"<<endl;
                else
                {
                    for(int i=0;i<lists.size();i++)
                    {
                        if(lists[i]==',')
                            lists[i]='\t';
                        else if(lists[i]==':')
                            lists[i]='\n';
                    }
                }
                cout<<lists<<endl;
            }
            else if(comps[0]=="accept_request" && comps.size()==3)
            {

                int size=0;
                string lists=send_receive(msg,my_sock,size);
                lists=lists.substr(0,size);
                cout<<lists<<endl;
            }
            else if(comps[0]=="list_files" && comps.size()==2)
            {
                int size=0;
                string lists=send_receive(msg,my_sock,size);
                lists=lists.substr(0,size);
                if(lists=="Invalid group" || lists=="No files are being shared")
                    cout<<lists<<endl;
                else
                {
                    for(int i=0;i<lists.size();i++)
                    {
                        if(lists[i]==',')
                            lists[i]='\n';
                    }
                    cout<<lists<<endl;
                }
            }
            else if(comps[0]=="upload_file" && comps.size()==3)
            {
                vector <string> reply=break_file(comps[1]);
                msg+=" ";
                for(int i=0;i<reply.size();i++)
                    msg+=reply[i]+" ";
                msg.pop_back();
                int size=0;
                string lists=send_receive(msg,my_sock,size);
                lists=lists.substr(0,size);
                cout<<lists<<endl;
            }
            else if(comps[0]=="download_file" && comps.size()==4)
            {
                if(find(my_groups.begin(),my_groups.end(),comps[1])==my_groups.end() &&find(own_groups.begin(),own_groups.end(),comps[1])==own_groups.end())
                {
                    cout<<"You are not a part of the group"<<endl;
                    continue;
                }
                string rec_hash="";
                class downloads *new_d=new downloads();
                new_d->fname=comps[2];
                new_d->gid=comps[1];
                new_d->state=0;
                cur_details->downloads->push_back(new_d);
                string cur_msg=comps[0]+" "+comps[1]+" "+comps[2];
                int size=0;
                string lists=send_receive(cur_msg,my_sock,size);
                lists=lists.substr(0,size);
                if(lists=="Invalid group" || lists=="You are not a part of the group" || lists=="File is not available")
                {
                    cout<<lists<<endl;
                    continue;
                }
                class chunk
                {
                    public:
                    string name;
                    string ip;
                    int port;
                    string hash;
                };
                vector <chunk> chunks;
                map <string,vector<pair<int,string>>> chunks_peer;
                set <string> chunk_names;
                string temp="";
                class chunk temp_chunk;
                //lists+=':';
                int pos=0;
                pos=lists.find_last_of(':');
                long long fsize=stoll(lists.substr(pos+1));
                lists=lists.substr(0,pos+1);
                int j=0;
                for(int i=0;i<lists.size();i++)
                {
                    if(lists[i]==':')
                    {
                        temp_chunk.hash=temp;
                        temp="";
                        chunks.push_back(temp_chunk);
                        if(chunks_peer.find(temp_chunk.name)==chunks_peer.end())
                        {
                            vector <pair<int,string>> new_peers;
                            new_peers.push_back(make_pair(temp_chunk.port,temp_chunk.ip));
                            chunks_peer[temp_chunk.name]=new_peers;
                        }
                        else
                            chunks_peer[temp_chunk.name].push_back(make_pair(temp_chunk.port,temp_chunk.ip));
                        j=0;
                        rec_hash+=temp_chunk.hash.substr(0,20);
                        if(chunk_names.find(temp_chunk.name)==chunk_names.end())
                            chunk_names.insert(temp_chunk.name);
                    }
                    else
                    {
                        if(lists[i]==',')
                        {
                            if(j==0)
                                temp_chunk.name=temp;
                            else if(j==1)
                                temp_chunk.ip=temp;
                            else if(j==2)
                                temp_chunk.port=stoi(temp);
                            else
                                temp_chunk.hash=temp;
                            temp="";
                            j++;
                        }
                        else
                            temp+=lists[i];
                    }
                }
                int num_chunks=(fsize/CHUNK_SZ)+((fsize%CHUNK_SZ==0)?0:1);
                int last_chunk_sz=fsize%CHUNK_SZ;
                pos=0;
                pos=comps[2].find_last_of('.');
                string name=comps[2].substr(0,pos);
                string ext=comps[2].substr(pos);
                pthread_t cts[num_chunks];
                int trs[num_chunks];
                char det[num_chunks][3000];
                int ch_sz=CHUNK_SZ;
                //num_chunks=1;
                string cur_name;
                for(int i=0;i<num_chunks;i++)
                {
                    if(i==num_chunks-1)
                        ch_sz=last_chunk_sz;
                    cur_name=name+"_"+to_string(i)+ext;
                    string t=cur_name+","+to_string(ch_sz)+","+to_string(chunks_peer[cur_name][0].first)+","+chunks_peer[cur_name][0].second;
                    strcpy(det[i],t.c_str());
                    trs[i]=pthread_create(&cts[i],NULL,receive_chunk,(void *)det[i]);
                }
                for(int i=0;i<num_chunks;i++)
                {
                    pthread_join(trs[i],NULL);
                    msg="has_chunk "+comps[1]+" "+comps[2]+" "+name+"_"+to_string(i)+ext;
                    string lists=send_receive(msg,my_sock,size);
                }
                cout<<"File Downloaded successfully"<<endl;
                cur_details->downloads->at(cur_details->downloads->size()-1)->state=1;
                cur_sha=assemble_file(comps[2],comps[3]);
                if(cur_sha==rec_hash)
                    cout<<"The hash of the files matches!!"<<endl;
                cout<<cur_sha<<endl;
                cout<<rec_hash<<endl;
            }
            else if(comps[0]=="stop_share" && comps.size()==3)
            {
                int size=0;
                string lists=send_receive(msg,my_sock,size);
                lists=lists.substr(0,size);
                cout<<lists<<endl;
            }
            else
            {
                cout<<"Invalid command"<<endl;
            }
        }
    }
    return 0;
}