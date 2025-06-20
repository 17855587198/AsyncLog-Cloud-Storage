#include<unordered_map>
#include "AsyncLogger.hpp"

/*日志管理器，负责统一管理多个异步日志器*/
namespace mylog{
     // 通过单例对象对日志器进行管理，懒汉模式
    class LoggerManager{
        public:
          static LoggerManager &GetInstance()//首次创建时创建实例
          {
            static LoggerManager eton;
            return eton;
          }

        bool LoggerExist(const std::string &name)
        {
            std::unique_lock<std::mutex> lck(mtx_);//使用互斥锁保护日志器的集合
            if(loggers_.find(name)!=loggers_.end())
                  return true;
            return false;
        }

        void AddLogger(AysncLogger::ptr &&logger)//函数明确接管参数的所有权（调用方放弃控制权),避免不必要的指针计数增减
        {
            if(LoggerExist(logger->Name()))
               return;//防止重复添加
            
            std::unique_lock<std::mutex>lck(mtx_);
            loggers_.emplace(std::make_pair(logger->Name(),std::move(logger)));//自动推导,emplace避免临时对象，emplace直接在容器中构造元素

        }

        AysncLogger::ptr GetLogger(const std::string &name)//获取日志器,从全局管理容器中按名称查找并返回对应的日志器对象
        {
            std::unique_lock<std::mutex>lck(mtx_);//加锁
            auto it=loggers_.find(name);
            if(it==loggers_.end())
                retrun nullptr;//未找到，返回空指针
            return it->second;
        }//找到，则返回对应的日志对象的指针

        AsyncLogger::ptr DefaultLogger(){return default_logger_;}
      
       private:
         LoggerManager(){
            std::unique_ptr<LoggerBuilder> builder(new LoggerBuilder());
            builder->BuildLoggerName("default");
            default_logger_=builder->Build();//创建日志器
            loggers_.emplace(std::make_pair("default",default_logger_));
        
         }
       private:
          std::mutex mtx_;
          AysncLogger::ptr default_logger_;//默认日志器对象，声明共享指针
          std::unordered_map<std::string,AysncLogger::ptr>loggers_;//日志器集合
    };
}