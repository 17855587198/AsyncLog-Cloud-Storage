#pragma once
#include<sys/stat.h>
#include<sys/types.h>
#include<jsoncpp/json/json.h>

#include<ctime>
#include<fstream>
#include<iostream>
using std::cout;
using std::endl;

namespace mylog
{
    namespace Util{
        class Date{//日期类
            public:
                static time_t Now(){return time(nullptr);}
        };
        /*文件操作工具类File：三种静态方法:检查文件，提取文件路径，递归创建目录*/

        class File{
            public:
               static bool Exists(const std::string &filename)//存在性
               {
                struct stat st;
                return (stat(filename.c_str(),&st)==0);//将std::string转换为const char *指针
               }

               static std::string Path(const std::string &filename)//文件路径
               {
                 if(filename.empty())
                    return "";
                  int pos=filename.find_last_of("/\\");//如/home/usr/file.txt,获取/home/usr/
                  if(pos!=std::string::npos)
                     return filename.substr(0,pos+1);//获取文件名
                   return "";
               }

                static void CreateDirectory(const std::string &pathname)//递归创建多级目录
                {
                    if(pathname.empty())
                    {
                        perror("文件所给路径为空：");
                    }
                    if(!Exists(pathname))//跳过已存在的目录
                    {
                        size_t pos,index=0;//当前解析的字符位置index和下一个路径分割符(/或\)的位置:pos
                        size_t size=pathname.size();
                        while(index<size)
                        {
                            pos=pathname.find_first_of("/\\",index);//index是起始搜索位置参数
                            if(pos==std::string::npos)//未找到分割符，即当前是最后一级目录
                            {
                                mkdir(pathname.c_str(),0755);//创建目录
                            }
                            if(pos==index)//跳过连续的分割符
                            {
                                index=pos+1;
                                continue;
                            }
                        
                          std::string sub_path=pathname.substr(0,pos);
                          if(sub_path=="."||sub_path=="..")//忽略当前目录和父目录,不创建，即./a/../b,.和..跳过
                          {
                            index=pos+1;
                            continue;
                          }
                          
                          if(Exists(sub_path))
                          {
                            index=pos+1;
                            continue;
                          }
                          mkdir(sub_path.c_str(),0755);
                          index=pos+1;
                        }
                    }
                }

                int64_t FileSize(std::string filename)
                {
                    struct stat s;
                    auto ret=stat(filename.c_str(),&s);
                    if(ret==-1)
                    {
                         perror("Get file size failed");
                         return -1;

                    }
                    return s.st_size;

                }

            bool GetContent(std::string *content,std::string filename)//获取文件
            {
                std::ifstream ifs;
                ifs.open(filename.c_str(),std::ios::binary);
                if(ifs.is_open()==false)
                {
                    std::cout << "file open error" << std::endl;
                    return false;
                }

                //读取对应的content
                ifs.seekg(0,std::ios::beg);//文件指针偏移修改
                size_t len=FileSize(filename);
                content->resize(len);
                ifs.read(&(*content)[0],len);
                if(!ifs.good())//判断文件是否损坏
                {
                    std::cout << __FILE__ << __LINE__ << "-"
                              << "read file content error" << std::endl;
                    ifs.close();
                    return false;

                }
                ifs.close();
                return true;
            }
        };

      class JsonUtil{
        public:
           static bool Serialize(const Json::Value &val,std::string *str)//输入Json::Value对象(val["name"]=Tom,val["age"]=24);输出序列化后的JSON字符串({"name": "Tom","age":24})
           {
             // 建造者生成->建造者实例化json写对象->调用写对象中的接口进行序列化写入str
              Json::StreamWriterBuilder swb;
                std::unique_ptr<Json::StreamWriter> usw(swb.newStreamWriter());
                std::stringstream ss;
                if (usw->write(val, &ss) != 0)
                {
                    std::cout << "serialize error" << std::endl;
                    return false;
                }
                *str = ss.str();
                return true;

           }
           static bool UnSerialize(const std::string &str,Json::Value *val)//用于将 JSON 格式的字符串解析为 Json::Value 对象(来自 JsonCpp 库）
           {
                Json::CharReaderBuilder crb;//JsonCpp 提供的解析器构建器，用于配置和生成 CharReader 对象
                std::unique_ptr<Json::CharReader> ucr(crb.newCharReader());//创建一个新的 JSON 解析器实例
                std::string err;
                if (ucr->parse(str.c_str(), str.c_str() + str.size(), val, &err) == false)
                {
                    std::cout <<__FILE__<<__LINE__<<"parse error" << err<<std::endl;
                    return false;
                }
                return false;

           }
      };
      /*单例模式的配置管理类，用于从JSON中配置文件加载日志系统的参数*/
        struct JsonData{
            static JsonData* GetJsonData()//加载config.conf到内存
            {
               static JsonData* json_data = new JsonData;
               return json_data;
            }//单例模式
            private:
                JsonData(){
                std::string content;
                mylog::Util::File file;
                if (file.GetContent(&content, "../../log_system/logs_code/config.conf") == false)//读取日志文件内容
                {
                    std::cout << __FILE__ << __LINE__ << "open config.conf failed" << std::endl;
                    perror(NULL);
                }
                Json::Value root;//解析json
                mylog::Util::JsonUtil::UnSerialize(content, &root); // 反序列化，反序列化字符串为 Json::Value 对象 root。
                buffer_size = root["buffer_size"].asInt64();//初始化成员变量
                threshold = root["threshold"].asInt64();//buffer_size和threshold决定日志缓冲区的内存管理策略
                linear_growth = root["linear_growth"].asInt64();
                flush_log = root["flush_log"].asInt64();
                backup_addr = root["backup_addr"].asString();
                backup_port = root["backup_port"].asInt();
                thread_count = root["thread_count"].asInt();
            }
            public:
                size_t buffer_size;//缓冲区基础容量
                size_t threshold;// 倍数扩容阈值
                size_t linear_growth;// 线性增长容量
                size_t flush_log;//控制日志同步到磁盘的时机，默认为0,1调用fflush，2调用fsync
                std::string backup_addr;
                uint16_t backup_port;
                size_t thread_count;
        };

    }
}


/*
创建的目录的逻辑:
CreateDirectory("project/logs/2023");
目录结构目标
最终要创建的目录结构：

text
./project/
    └── logs/
        └── 2023/
代码执行步骤详解
初始状态
输入路径："project/logs/2023"

变量初始化：

index = 0（从路径的第 0 个字符开始解析）

size = 16（字符串长度）

第 1 次循环：处理 project/
查找分隔符

cpp
pos = pathname.find_first_of("/\\", 0); // 查找第一个 `/` 或 `\`
找到 / 的位置 pos = 7（"project/" 中的 /）。

提取子路径

cpp
std::string sub_path = pathname.substr(0, 7); // "project"
不是 . 或 ..，继续执行。

检查目录是否存在

cpp
if (!Exists("project")) {
    mkdir("project", 0755); // 创建 project/
}
如果 project/ 不存在，则创建。

更新索引

cpp
index = pos + 1; // index = 8（指向 `logs/2023` 的 `l`）
第 2 次循环：处理 logs/
查找分隔符

cpp
pos = pathname.find_first_of("/\\", 8); // 查找下一个 `/`
找到 / 的位置 pos = 12（"logs/" 中的 /）。

提取子路径

cpp
std::string sub_path = pathname.substr(0, 12); // "project/logs"
不是 . 或 ..，继续执行。

检查目录是否存在

cpp
if (!Exists("project/logs")) {
    mkdir("project/logs", 0755); // 创建 project/logs/
}
如果 project/logs/ 不存在，则创建。

更新索引

cpp
index = pos + 1; // index = 13（指向 `2023` 的 `2`）
第 3 次循环：处理 2023
查找分隔符

cpp
pos = pathname.find_first_of("/\\", 13); // 查找下一个 `/`
未找到分隔符，pos = std::string::npos。

创建最后一级目录

cpp
if (pos == std::string::npos) {
    mkdir("project/logs/2023", 0755); // 创建 project/logs/2023/
}
直接创建完整路径。

循环结束

index 不再小于 size，退出循环。
*/