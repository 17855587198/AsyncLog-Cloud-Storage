#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

#include "AsyncBuffer.hpp"

/*
AsyncWorker 是一个异步工作器类，主要用于实现生产者-消费者模式下的异步日志记录功能。
(1)接收生产者线程推送的数据（如日志消息）;
(2)将数据缓冲在内存中;
(3)在适当的时机（缓冲区满或主动刷新时）将缓冲数据通过回调函数写入目标（如文件);
*/

namespace mylog
{
    enum class AsyncType { ASYNC_SAFE, ASYNC_UNSAFE };//安全模式,即缓冲区满阻塞生产者;非安全模式则不阻塞生产者
    using functor=std::function<void(Buffer&)>;//别名
    class Asyncbuffer{
    public:
    using ptr=std::shared_ptr<AsyncWorker>;
     AsyncWorker(const functor& cb, AsyncType async_type = AsyncType::ASYNC_SAFE)
        : async_type_(async_type),
          callback_(cb),
          stop_(false),
          thread_(std::thread(&AsyncWorker::ThreadEntry, this)) {}//该线程持续运行，直到stop_被设置为true且所有的缓冲区数据被处理完毕
    ~AsyncWorker() { Stop(); }
     void Push(const char *data,size_t len)//生产者接口
     {
        std::unique_lock<std::mutex>lock(mtx_);//加锁修改缓冲区
        if (AsyncType::ASYNC_SAFE == async_type_)// 在安全模式下可能会被阻塞，如果生产者队列不足以写下len长度数据，并且缓冲区是固定大小，那么阻塞
            cond_productor_.wait(lock, [&]() {
                return len <= buffer_productor_.WriteableSize();
            });
        buffer_productor_.Push(data,len);//生产数据
        cond_consumer_.notify_one();
     }
    
    void stop(){
        stop_=true;
        cond_consumer_.notify_all(); //所有线程把缓冲区内数据处理完就结束了
        thread_.join();//线程加入执行
    }
    private:
        
        void ThreadEntry()//工作线程执行入口
        {
          while(1)
          {
            std::unique_lock<std::mutex>lock(mtx_);
            if(buffer_productor_.IsEmpty()&&stop_) //有数据则交换（进行消费），无数据就阻塞
            {
                 cond_consumer_.wait(lock, [&]() {
                        return stop_ || !buffer_productor_.IsEmpty();});
                buffer_productor_.Swap(buffer_consumer_);
              if (async_type_ == AsyncType::ASYNC_SAFE)//在安全模式下，若生产者由于之前因缓冲区满而被阻塞就会唤醒
                    cond_productor_.notify_one();
            }
            callback_(buffer_consumer_);//回调函数，传入Buffer对象
            buffer_consumer_.Reset();
            if(stop_&&buffer_productor_.IsEmpty())
               return;//生产缓冲区空退出
          }
        }
        AsyncType async_type_;
        std::atomic<bool> stop_;  // 用于控制异步工作器的启动
        std::mutex mtx_;
        mylog::Buffer buffer_productor_;//分别定义消费者缓冲区和生产者缓冲区
        mylog::Buffer buffer_consumer_;
        std::condition_variable cond_productor_;
        std::condition_variable cond_consumer_;
        std::thread thread_;

        functor callback_;  // (使用绑定器定义类型的)回调函数，用来告知工作器如何落地


    };
}
/*
代码 callback_(buffer_consumer_); 中，
将类的对象 buffer_consumer_ 传入回调函数 callback_，
其核心意义在于 实现解耦和灵活扩展。
具体来说，这是典型的 回调（Callback）设计模式 的应用，
允许外部代码在特定时机介入处理类的内部数据。
buffer_consumer_ 是类内部生成或管理的数据对象（如缓冲区、网络数据包等），
但类本身不直接处理这些数据，
而是通过回调交给外部用户定义的处理逻辑。
callback_ 是一个 回调函数（Callback），但它本身只是一个 容器（通过 std::function 封装），
其具体执行逻辑需要由外部代码定义并注入。
回调函数的核心特点是 “定义权在外部，调用权在内部”。

你的代码：callback_(buffer_consumer_) 是 调用方（触发回调的执行）。

外部代码：需要提前 定义回调的具体逻辑 并通过 setCallback 等方法注入。
callback_ 是一个 抽象接口，它的目的是让类内部不关心具体逻辑，只负责在特定时机（如数据准备好时）调用它
*/

