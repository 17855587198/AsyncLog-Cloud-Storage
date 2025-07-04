#include "CoroutineStyleAsyncLogger.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>

using namespace mylog::coroutine_style;

// 演示协程风格异步日志的完整流程
class LogFlowDemo {
public:
    static void demonstrate() {
        std::cout << "=== 协程风格异步日志系统流程演示 ===" << std::endl;
        
        // 第1步：系统初始化
        std::cout << "\n[步骤1] 系统初始化..." << std::endl;
        CoroutineLogManager::getInstance().initialize(4); // 4个工作线程
        std::cout << "   ✓ TaskScheduler 启动完成，工作线程数: 4" << std::endl;
        
        // 第2步：创建输出函数
        std::cout << "\n[步骤2] 创建输出函数..." << std::endl;
        std::vector<CoroutineStyleAsyncLogger::FlushFunction> flushers;
        
        // 控制台输出函数
        flushers.push_back([](const std::string& data) {
            std::cout << "[控制台输出] " << data;
            std::cout.flush();
        });
        
        // 文件输出函数
        flushers.push_back([](const std::string& data) {
            std::ofstream file("flow_demo.log", std::ios::app);
            if (file.is_open()) {
                file << "[文件输出] " << data;
                file.flush();
            }
        });
        
        std::cout << "   ✓ 创建了 " << flushers.size() << " 个输出函数" << std::endl;
        
        // 第3步：创建日志器
        std::cout << "\n[步骤3] 创建异步日志器..." << std::endl;
        auto logger = std::make_shared<CoroutineStyleAsyncLogger>("FlowDemo", flushers);
        std::cout << "   ✓ 日志器创建完成，消费者协程已启动" << std::endl;
        
        // 第4步：注册日志器
        std::cout << "\n[步骤4] 注册日志器到管理器..." << std::endl;
        CoroutineLogManager::getInstance().add_logger("demo", logger);
        std::cout << "   ✓ 日志器注册完成" << std::endl;
        
        // 第5步：演示异步日志写入流程
        std::cout << "\n[步骤5] 演示异步日志写入流程..." << std::endl;
        demonstrateAsyncFlow(logger);
        
        // 第6步：演示高并发场景
        std::cout << "\n[步骤6] 演示高并发日志写入..." << std::endl;
        demonstrateConcurrentLogging(logger);
        
        // 第7步：系统关闭
        std::cout << "\n[步骤7] 系统关闭..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2)); // 等待所有日志处理完成
        CoroutineLogManager::getInstance().shutdown();
        std::cout << "   ✓ 系统关闭完成" << std::endl;
    }
    
private:
    static void demonstrateAsyncFlow(std::shared_ptr<CoroutineStyleAsyncLogger> logger) {
        std::cout << "\n--- 单个日志的异步处理流程 ---" << std::endl;
        
        // 创建一个future来跟踪处理过程
        std::cout << "1. 用户调用 log_async()" << std::endl;
        auto future = logger->log_async("INFO", "这是一条演示日志消息");
        
        std::cout << "2. 立即返回 future，日志在后台异步处理" << std::endl;
        std::cout << "3. 用户可以继续执行其他操作..." << std::endl;
        
        // 模拟用户继续执行其他操作
        for (int i = 0; i < 5; ++i) {
            std::cout << "   执行其他操作 " << (i + 1) << "..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "4. 等待日志处理完成..." << std::endl;
        future.wait();
        std::cout << "   ✓ 日志处理完成！" << std::endl;
    }
    
    static void demonstrateConcurrentLogging(std::shared_ptr<CoroutineStyleAsyncLogger> logger) {
        std::cout << "\n--- 并发日志处理演示 ---" << std::endl;
        
        const int log_count = 100;
        std::vector<std::future<void>> futures;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 并发提交大量日志
        for (int i = 0; i < log_count; ++i) {
            std::string message = "并发日志消息 #" + std::to_string(i);
            futures.push_back(logger->log_async("INFO", message));
        }
        
        std::cout << "提交了 " << log_count << " 条日志，等待处理完成..." << std::endl;
        
        // 等待所有日志处理完成
        for (auto& future : futures) {
            future.wait();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "✓ 所有日志处理完成，耗时: " << duration.count() << " 毫秒" << std::endl;
        std::cout << "平均处理速度: " << (log_count * 1000.0 / duration.count()) << " 条/秒" << std::endl;
    }
};

// 详细展示内部流程的类
class InternalFlowTracker {
public:
    static void trackTaskSchedulerFlow() {
        std::cout << "\n=== TaskScheduler 内部流程追踪 ===" << std::endl;
        
        auto& scheduler = TaskScheduler::getInstance();
        
        std::cout << "1. 提交任务到调度器..." << std::endl;
        auto future = scheduler.async_execute([]() {
            std::cout << "   → 任务开始执行（在工作线程中）" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::cout << "   → 任务执行完成" << std::endl;
        });
        
        std::cout << "2. 任务已提交，等待执行..." << std::endl;
        future.wait();
        std::cout << "3. 任务执行完成！" << std::endl;
    }
    
    static void trackBufferFlow() {
        std::cout << "\n=== AsyncBuffer 内部流程追踪 ===" << std::endl;
        
        AsyncBuffer buffer;
        
        std::cout << "1. 异步写入数据..." << std::endl;
        auto write_future = buffer.push_async("测试数据");
        
        std::cout << "2. 异步读取数据..." << std::endl;
        auto read_future = buffer.pop_async();
        
        std::cout << "3. 等待写入完成..." << std::endl;
        write_future.wait();
        
        std::cout << "4. 等待读取完成..." << std::endl;
        auto data = read_future.get();
        std::cout << "   读取到的数据: " << data << std::endl;
        
        buffer.stop();
    }
};

int main() {
    try {
        // 主流程演示
        LogFlowDemo::demonstrate();
        
        // 内部流程追踪
        InternalFlowTracker::trackTaskSchedulerFlow();
        InternalFlowTracker::trackBufferFlow();
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
