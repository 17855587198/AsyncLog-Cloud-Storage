#pragma once
#include "Manager.hpp"

/*日志系统的用户接口封装:简化日志库的调用方式，同时提供灵活性和易用性*/
namespace mylog {
// 用户获取日志器
AsyncLogger::ptr GetLogger(const std::string &name) {
    return LoggerManager::GetInstance().GetLogger(name);
}
// 用户获取默认日志器
AsyncLogger::ptr DefaultLogger() { return LoggerManager::GetInstance().DefaultLogger(); }

// 简化用户使用，宏函数默认填上文件名+行号(使用前手动获取日志对象,再调用对应的方法)
#define Debug(fmt, ...) Debug(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Info(fmt, ...) Info(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Warn(fmt, ...) Warn(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Error(fmt, ...) Error(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Fatal(fmt, ...) Fatal(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

// 无需获取日志器，默认标准输出(无需手动创建日志器，直接使用全局默认日志器（由 LoggerManager 在程序启动时初始化）)
#define LOGDEBUGDEFAULT(fmt, ...) mylog::DefaultLogger()->Debug(fmt, ##__VA_ARGS__)
#define LOGINFODEFAULT(fmt, ...) mylog::DefaultLogger()->Info(fmt, ##__VA_ARGS__)
#define LOGWARNDEFAULT(fmt, ...) mylog::DefaultLogger()->Warn(fmt, ##__VA_ARGS__)
#define LOGERRORDEFAULT(fmt, ...) mylog::DefaultLogger()->Error(fmt, ##__VA_ARGS__)
#define LOGFATALDEFAULT(fmt, ...) mylog::DefaultLogger()->Fatal(fmt, ##__VA_ARGS__)
}  // namespace mylog

/*
使用:
auto net_logger = mylog::GetLogger("Network");
net_logger->Debug("Connection attempt started");  // 调试信息

Debug("Connection attempt started"); //使用使用宏简化操作


*/