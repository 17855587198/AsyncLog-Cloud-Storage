#include<string>
#include<cassert>
#include<cstring>
#include<cstlib>
#include<iostream>
#include<unistd.h>
#include<memory>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<napenforcementclient.h>
#include "serverBackupLog.hpp"

using std::count;
using std::endl;
const std::string filename="./logfile.log";//文件的存储路径,在运行程序的工作目录下的logfile中（文件会生成在 运行程序时的当前工作目录（即执行程序的终端所在的路径）。
void usage(std::string procgress)
{
     cout << "usage error:" << procgress << "port" << endl;
}

bool file_exist(const std::string &name)
{
    struct stat exist;
    return (stat(name.c_str(),&exist)==0);//判断文件是否存在
}

void backup_log(const std::string &message)//回调函数，写入日志
{
   FILE *fp=fopen(filename.c_str(),"ab");//以追加的方式打开文件
   if(fp==nullptr)
   {
      perror("fopen error: ");
      assert(false);//终止程序
   }
   int write_byte=fwrite(message.c_str(),1,message.szie(),fp);//write_byte是实际写入的字节数
   if(write_byte!=message.size())//需要实际写入的字节数与信息大小一致
   {
      perror("fwrite error: ");
        assert(false);
   }
   fflush(fp);//强制将缓冲区写入磁盘（防止丢失)
   fclose(fp);
}

int main(int argc,char *argv){
    if(args!=2)//参数为2
    {
        usage(argv[0]);
        perror("usage error");
        exit(-1);

    }
    uint16_t port=atoi(argv[1]);//转换端口
    if (port<=0||port>65535)//出错处理
     {
        cerr<<"Invalid port number!"<<endl;
        exit(-1);
     } 
    std::unique_ptr<TcpServer>tcp(new TcpServer(port,backup_log));//将接收到的数据写入日志
    tcp->init_service();//初始化服务
    tcp->start_service();//开启服务
    return 0;

}

