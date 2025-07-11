#pragma once
#include<iostream>
#include<string>
#include<unistd.h>
#include<string>
#include<cerrno>
#include<cstdlib>
#include<pthread.h>
#include<mutex>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<functional>

using std::cout;
using std::endl;

using func_t=std::functon<void(const std::string&)>;//包装器定义类型
const int backlog=32;

class TcpServer;

class ThreadData//封装线程处理客户端连接所需的所有数据
{
   public:
       ThreadData(int fd, const std::string &ip, const uint16_t &port, TcpServer *ts)
        : sock(fd),client_ip(ip),client_port(port),ts_(ts){}//构造
      int sock;//cfd
      std::string client_ip;//客户端ip
      uint16_t client_port;//客户端端口
      TcpServer *ts_;//调用服务器对象的服务方法

};

class TcpServer{
    public:
     TcpServer(uint16_t port,func_t func):
     {

     }
     void init_service(){
        listen_sock_=socket(AF_INET,SOCK_STREAM,0);//lfd
        if (listen_sock_ == -1){
            std::cout << __FILE__ << __LINE__ <<"create socket error"<< strerror(errno)<< std::endl;
        }
        struct sockaddr_in local_addr;//server addr
        local_addr.sin_family=AF_INET;
        local sin_port=htons(port_);
        local_addr.sin_addr.s_addr=htonl(INADDR_ANY);

        if(bind(listen_sock_,(struct sockaddr*)&local_addr,sizeof(local_addr))<0)
        {
             std::cout << __FILE__ << __LINE__ << "bind socket error"<< strerror(errno)<< std::endl;

        }
        
        if(listen(listen_sock_,backlog,0)<0)
        {
            std::cout << __FILE__ << __LINE__ <<  "listen error"<< strerror(errno)<< std::endl;
        }
     }

        static void *threadRoutine(void *args)
        {
            pthread_detach(pthread_self());//防止在start_service处阻塞
            ThreadData *td=static_cast<ThreadData *>(args);
            std::string client_info = td->client_ip + ":" + std::to_string(td->client_port);//将客户端的信息进行拼接
            td->ts_->service(td->sock,move(client_info));
            close(td->sock);
            delete td;
            return nullptr;
        }

        void start_service(){
            while(true)
            {
                struct sockaddr_in client_addr;
                socklen_t client_adrlen=sizeof(client_addr);
                int connfd = accept(listen_sock_, (struct sockaddr *)&client_addr, &client_addrlen);
                if(connfd<0){
                    std::cout << __FILE__ << __LINE__ << "accept error"<< strerror(errno)<< std::endl;
                    continue;
                }
            /* ​​服务端并通过 accept() 系统调用,​​被动获取​​ 客户端的连接信息*/
            std::string client_ip=inet_ntoa(client_addr.sin_addr);//获取客户端ip
            uint16_t client_port=ntohs(client_addr.sin_port);

             // 多个线程提供服务
            // 传入线程数据类型来访问threadRoutine，因为该函数是static的，所以内部传入了data类型存了tcpserver类型
            pthread_t tid;
            ThreadData *td = new ThreadData(connfd, client_ip, client_port, this);
            pthread_create(&tid, nullptr, threadRoutine, td);//将td作为参数传给threadRoutine
            }
        }
         
        void service(int sock,const std::string &&client_info)
        {
            char buf[1024];
            int r_ret=raed(sock,buf,sizeof(buf));
            if(r_ret==-1)
            {
                std::cout << __FILE__ << __LINE__ <<"read error"<< strerror(errno)<< std::endl;
                perror("NULL");
            }
            else if(r_ret > 0){
            buf[r_ret] = 0;
            std::string tmp = buf;
            func_(client_info+tmp); // 进行回调
          }

        }   
     ~TcpServer()=default;
    
    private:
       int listen_sock_;//lfd
        uint16_t port;//端口
        func_t func_;//回调函数

};