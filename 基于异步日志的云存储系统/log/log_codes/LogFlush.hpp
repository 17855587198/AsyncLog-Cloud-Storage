#include <cassert>
#include <fstream>
#include <memory>
#include <unistd.h>
#include "Util.hpp"
/*
实现日志系统的输出模块，
提供了多种日志落地方式（标准输出、普通文件、滚动文件），
并采用了工厂模式来创建不同的日志输出器。
*/
extern mylog::Util::JsonData* g_conf_data;//声明变量 g_conf_data ​​在其他文件中定义​​（当前文件仅声明，不分配内存）
namespace mylog{
    class LogFlush
    {
    public:
        using ptr = std::shared_ptr<LogFlush>;
        virtual ~LogFlush() {}
        virtual void Flush(const char *data, size_t len) = 0;//不同的写文件方式Flush的实现不同
    };

    class StdoutFlush : public LogFlush//日志输出到标准输出
    {
    public:
        using ptr = std::shared_ptr<StdoutFlush>;
        void Flush(const char *data, size_t len) override{
            cout.write(data, len);
        }
    };//直接将日志内容写入到标准输出std::cout
    /*
    将日志写入单个固定文件
        支持三种刷盘策略：
            0: 依赖系统缓冲
            1: 刷新C库缓冲区(fflush)
            2: 强制写入磁盘(fsync)
            自动创建所需目录结构
    */
    class FileFlush : public LogFlush //日志输出到普通的文件
    {
    public:
        using ptr = std::shared_ptr<FileFlush>;
        FileFlush(const std::string &filename) : filename_(filename)
        {
            // 创建所给目录
            Util::File::CreateDirectory(Util::File::Path(filename));
            // 打开文件
            fs_ = fopen(filename.c_str(), "ab");
            if(fs_==NULL){
                std::cout <<__FILE__<<__LINE__<<"open log file failed"<< std::endl;
                perror(NULL);
            }
        }
        void Flush(const char *data, size_t len) override{
            fwrite(data,1,len,fs_);
            if(ferror(fs_))//ferror检查文件操作是否出错
            {
                std::cout <<__FILE__<<__LINE__<<"write log file failed"<< std::endl;
                perror(NULL);
            }
            if(g_conf_data->flush_log == 1)//刷新C库缓冲区
            {
                if(fflush(fs_)==EOF)//fflush是将C标准库写入操作系统,成功返回0，失败返回EOF
                {
                    std::cout <<__FILE__<<__LINE__<<"fflush file failed"<< std::endl;
                    perror(NULL);
                }
            }else if(g_conf_data->flush_log == 2){
                fflush(fs_);
                fsync(fileno(fs_));//fsync是强制写入C盘,开销很大，需要磁盘IO成功
            }
        }

    private:
        std::string filename_;
        FILE* fs_ = NULL; 
    };

    class RollFileFlush : public LogFlush//日志输出到文件,并按大小生成日志文件
    {
    public:
        using ptr = std::shared_ptr<RollFileFlush>;
        RollFileFlush(const std::string &filename, size_t max_size)
            : max_size_(max_size), basename_(filename)
        {
            Util::File::CreateDirectory(Util::File::Path(filename));
        }

        void Flush(const char *data, size_t len) override
        {
            // 确认文件大小不满足滚动需求
            InitLogFile();
            // 向文件写入内容
            fwrite(data, 1, len, fs_);
            if(ferror(fs_)){
                std::cout <<__FILE__<<__LINE__<<"write log file failed"<< std::endl;
                perror(NULL);
            }
            cur_size_ += len;//同FileFlush的刷盘策略
            if(g_conf_data->flush_log == 1){
                if(fflush(fs_)){
                    std::cout <<__FILE__<<__LINE__<<"fflush file failed"<< std::endl;
                    perror(NULL);
                }
            }else if(g_conf_data->flush_log == 2){
                fflush(fs_);
                fsync(fileno(fs_));
            }
        }

    private:
        void InitLogFile()
        {
            if (fs_==NULL || cur_size_ >= max_size_)
            {
                if(fs_!=NULL){
                    fclose(fs_);
                    fs_=NULL;
                }   
                std::string filename = CreateFilename();
                fs_=fopen(filename.c_str(), "ab");
                if(fs_==NULL){
                    std::cout <<__FILE__<<__LINE__<<"open file failed"<< std::endl;
                    perror(NULL);
                }
                cur_size_ = 0;
            }
        }

        // 构建落地的滚动日志文件名称
        std::string CreateFilename()// 生成带时间戳和序号的文件名
        // 格式: basename_YYYYMMDDHHMMSS-N.log
        {
            time_t time_ = Util::Date::Now();
            struct tm t;
            localtime_r(&time_, &t);
            std::string filename = basename_;
            filename += std::to_string(t.tm_year + 1900);
            filename += std::to_string(t.tm_mon + 1);
            filename += std::to_string(t.tm_mday);
            filename += std::to_string(t.tm_hour + 1);
            filename += std::to_string(t.tm_min + 1);
            filename += std::to_string(t.tm_sec + 1) + '-' +
                        std::to_string(cnt_++) + ".log";
            return filename;
        }
    private:
        size_t cnt_ = 1;
        size_t cur_size_ = 0;
        size_t max_size_;
        std::string basename_;
        // std::ofstream ofs_;
        FILE* fs_ = NULL;
    };

    class LogFlushFactory
    {
    public:
        using ptr = std::shared_ptr<LogFlushFactory>;
        template <typename FlushType, typename... Args>
        static std::shared_ptr<LogFlush> CreateLog(Args &&...args)
        {
            return std::make_shared<FlushType>(std::forward<Args>(args)...);
        }
    };
} // namespace mylog
