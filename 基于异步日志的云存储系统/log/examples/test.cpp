#include "../logs_code/MyLog.hpp"
#include "../logs_code/ThreadPoll.hpp"
#include "../logs_code/Util.hpp"
#include <chrono>
#include <iostream>
using std::cout;
using std::endl;

ThreadPool* tp=nullptr;
mylog::Util::JsonData* g_conf_data;
void test() {
    const int test_count = 10000;  // 增加测试数量
    
    std::cout << "开始性能测试，消息数量: " << test_count << std::endl;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < test_count; ++i) {
        mylog::GetLogger("asynclogger")->Info("性能测试日志-%d", i);
        
        // 每1000条打印一次进度
        if (i % 1000 == 0) {
            std::cout << "已处理: " << i << " 条日志" << std::endl;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "性能测试完成!" << std::endl;
    std::cout << "总耗时: " << duration.count() << " ms" << std::endl;
    std::cout << "吞吐量: " << (test_count * 1000.0 / duration.count()) << " msg/s" << std::endl;
}

void init_thread_pool() {
    tp = new ThreadPool(g_conf_data->thread_count);
}
int main() {
    g_conf_data = mylog::Util::JsonData::GetJsonData();
    init_thread_pool();
    std::shared_ptr<mylog::LoggerBuilder> Glb(new mylog::LoggerBuilder());
    Glb->BuildLoggerName("asynclogger");
    Glb->BuildLoggerFlush<mylog::FileFlush>("./logfile/FileFlush.log");
    Glb->BuildLoggerFlush<mylog::RollFileFlush>("./logfile/RollFile_log",
                                              1024 * 1024);
    //建造完成后，日志器已经建造，由LoggerManger类成员管理诸多日志器
    // 把日志器给管理对象，调用者通过调用单例管理对象对日志进行落地
    mylog::LoggerManager::GetInstance().AddLogger(Glb->Build());
    test();
    delete(tp);
    return 0;
}