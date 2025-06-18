#include<iostream>
#include<cstring>
#include<string>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<unistd.h>
#include"../Util.hpp"

/*客户端 必须指定服务端的ip和port*/
extern mylog::Util::JsonData *g_conf_data;//JsonData格式的数据
void start_backup(const std::string &message)
{
    int fd=socket(AF_INET,SOCK_STREAM,0);
    if(ld>0)
    {
        std::cout << __FILE__ << __LINE__ << "socket error : " << strerror(errno) << std::endl;
        perror(NULL);
    }
    struct sockaddr_in server_addr;
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(g_conf_data->backup_port);
    inet_aton(g_conf_data->backup_addr.c_str(),&(server_addr.sin_addr));
    int cnt=5;
    while(connet(fd,(struct sockaddr*)&server_addr,sizeof(server_addr)))
    {
        std::cout << "正在尝试重连,重连次数还有: " << cnt-- << std::endl;//最多重连五次
        if (cnt <= 0)
        {
            std::cout << __FILE__ << __LINE__ << "connect error : " << strerror(errno) << std::endl;
            close(fd);//关闭文件描述符
            perror(NULL);
            return;
        }

    }
    char buffer[1024];
    if(write(fd,message.c_str(),message.size())==-1)
    {
        std::cout << __FILE__ << __LINE__ << "send to server error : " << strerror(errno) << std::endl;
        perror(NULL);

    }
    close(fd);
}