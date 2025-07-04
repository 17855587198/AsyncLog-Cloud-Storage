#include "CoroutineStyleAsyncLogger.hpp"
#include <iostream>
#include <fstream>
#include <unordered_map>

using namespace mylog::coroutine_style;

// 文件输出函数
void file_flusher(const std::string& filename) {
    return [filename](const std::string& data) {
        std::ofstream file(filename, std::ios::app);
        if (file.is_open()) {
            file << data;
            file.flush();
        }
    };
}

// 控制台输出函数
void console_flusher(const std::string& data) {
    std::cout << data;
    std::cout.flush();
}

int main() {
    // 初始化协程调度器
    CoroutineLogManager::getInstance().initialize(4);
    
    // 创建日志输出器
    std::vector<CoroutineStyleAsyncLogger::FlushFunction> flushers;
    flushers.push_back(console_flusher);
    flushers.push_back(file_flusher("coroutine_log.txt"));
    
    // 创建协程风格的异步日志器
    auto logger = std::make_shared<CoroutineStyleAsyncLogger>("main", flushers);
    
    // 注册日志器
    CoroutineLogManager::getInstance().add_logger("main", logger);
    
    // 测试日志输出
    std::cout << "开始测试协程风格异步日志系统..." << std::endl;
    
    // 并发日志写入测试
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 1000; ++i) {
        futures.push_back(logger->log_async("INFO", "测试消息 " + std::to_string(i)));
        
        if (i % 100 == 0) {
            futures.push_back(logger->log_async("DEBUG", "调试消息 " + std::to_string(i)));
        }
        
        if (i % 200 == 0) {
            futures.push_back(logger->log_async("WARN", "警告消息 " + std::to_string(i)));
        }
        
        if (i % 300 == 0) {
            futures.push_back(logger->log_async("ERROR", "错误消息 " + std::to_string(i)));
        }
    }
    
    // 等待所有日志写入完成
    for (auto& future : futures) {
        future.wait();
    }
    
    std::cout << "所有日志已写入完成！" << std::endl;
    
    // 等待一秒确保所有数据都被刷盘
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 关闭日志系统
    CoroutineLogManager::getInstance().shutdown();
    
    return 0;
}
