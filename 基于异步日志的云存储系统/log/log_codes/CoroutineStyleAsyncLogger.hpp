/*
 * 基于协程的异步日志系统 - 使用现有C++特性的实现
 * 
 * 这个实现展示了如何使用协程概念来改进现有的异步日志系统
 * 主要改进：
 * 1. 使用更轻量级的协程调度替代线程池
 * 2. 简化异步操作的复杂度
 * 3. 更好的资源管理
 */

#pragma once
#include <future>
#include <functional>
#include <queue>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <iostream>

namespace mylog {
namespace coroutine_style {

// 模拟协程的任务调度器
class TaskScheduler {
public:
    using Task = std::function<void()>;
    
    static TaskScheduler& getInstance() {
        static TaskScheduler instance;
        return instance;
    }
    
    // 异步执行任务（void返回类型）
    template<typename F>
    auto async_execute(F&& func) -> typename std::enable_if<std::is_void<typename std::result_of<F()>::type>::value, std::future<void>>::type {
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            task_queue_.push([func = std::forward<F>(func), promise]() {
                try {
                    func();
                    promise->set_value();
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            });
        }
        
        condition_.notify_one();
        return future;
    }
    
    // 异步执行任务（非void返回类型）
    template<typename F>
    auto async_execute(F&& func) -> typename std::enable_if<!std::is_void<typename std::result_of<F()>::type>::value, std::future<typename std::result_of<F()>::type>>::type {
        using return_type = typename std::result_of<F()>::type;
        auto promise = std::make_shared<std::promise<return_type>>();
        auto future = promise->get_future();
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            task_queue_.push([func = std::forward<F>(func), promise]() {
                try {
                    auto result = func();
                    promise->set_value(result);
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            });
        }
        
        condition_.notify_one();
        return future;
    }
    
    // 延迟执行任务
    template<typename F>
    auto async_execute_delayed(F&& func, std::chrono::milliseconds delay) -> std::future<void> {
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();
        
        std::thread([func = std::forward<F>(func), promise, delay]() {
            std::this_thread::sleep_for(delay);
            try {
                func();
                promise->set_value();
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        }).detach();
        
        return future;
    }
    
    // 启动调度器
    void start(size_t thread_count = std::thread::hardware_concurrency()) {
        stop_ = false;
        for (size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this]() {
                while (!stop_) {
                    Task task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex_);
                        condition_.wait(lock, [this] { return stop_ || !task_queue_.empty(); });
                        
                        if (stop_ && task_queue_.empty()) {
                            break;
                        }
                        
                        task = std::move(task_queue_.front());
                        task_queue_.pop();
                    }
                    task();
                }
            });
        }
    }
    
    // 停止调度器
    void stop() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        
        for (auto& worker : workers_) {
            worker.join();
        }
        workers_.clear();
    }
    
private:
    TaskScheduler() = default;
    ~TaskScheduler() { stop(); }
    
    std::queue<Task> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::vector<std::thread> workers_;
    std::atomic<bool> stop_{false};
};

// 协程风格的异步缓冲区
class AsyncBuffer {
public:
    AsyncBuffer(size_t initial_size = 8192) 
        : buffer_(initial_size), write_pos_(0), read_pos_(0) {}
    
    // 异步写入数据
    std::future<void> push_async(const std::string& data) {
        return TaskScheduler::getInstance().async_execute([this, data]() {
            std::unique_lock<std::mutex> lock(mutex_);
            
            // 等待空间可用
            write_cv_.wait(lock, [this, &data]() {
                return write_pos_ + data.size() <= buffer_.size() || !running_;
            });
            
            if (!running_) return;
            
            // 扩展缓冲区如果需要
            if (write_pos_ + data.size() > buffer_.size()) {
                buffer_.resize(std::max(buffer_.size() * 2, write_pos_ + data.size()));
            }
            
            // 写入数据
            std::copy(data.begin(), data.end(), buffer_.begin() + write_pos_);
            write_pos_ += data.size();
            
            // 通知读取者
            read_cv_.notify_one();
        });
    }
    
    // 异步读取数据
    std::future<std::string> pop_async() {
        return TaskScheduler::getInstance().async_execute([this]() -> std::string {
            std::unique_lock<std::mutex> lock(mutex_);
            
            // 等待数据可用
            read_cv_.wait(lock, [this]() {
                return write_pos_ > read_pos_ || !running_;
            });
            
            if (!running_ && write_pos_ == read_pos_) {
                return "";
            }
            
            // 读取数据
            std::string result(buffer_.begin() + read_pos_, buffer_.begin() + write_pos_);
            reset_buffer();
            
            // 通知写入者
            write_cv_.notify_one();
            
            return result;
        });
    }
    
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }
        write_cv_.notify_all();
        read_cv_.notify_all();
    }
    
private:
    void reset_buffer() {
        write_pos_ = 0;
        read_pos_ = 0;
    }
    
    std::mutex mutex_;
    std::condition_variable write_cv_;
    std::condition_variable read_cv_;
    std::vector<char> buffer_;
    size_t write_pos_;
    size_t read_pos_;
    std::atomic<bool> running_{true};
};

// 协程风格的异步日志器
class CoroutineStyleAsyncLogger {
public:
    using FlushFunction = std::function<void(const std::string&)>;
    
    CoroutineStyleAsyncLogger(const std::string& name, 
                             std::vector<FlushFunction> flushers)
        : name_(name), flushers_(std::move(flushers)), running_(true) {
        
        // 启动异步消费者
        start_consumer();
    }
    
    ~CoroutineStyleAsyncLogger() {
        stop();
    }
    
    // 异步日志写入
    std::future<void> log_async(const std::string& level, const std::string& message) {
        if (!running_) {
            auto promise = std::promise<void>();
            promise.set_value();
            return promise.get_future();
        }
        
        std::string formatted = format_message(level, message);
        return buffer_.push_async(formatted);
    }
    
    // 同步日志写入接口
    void log(const std::string& level, const std::string& message) {
        auto future = log_async(level, message);
        // 对于同步接口，我们可以选择等待或者不等待
        // 这里为了性能，选择不等待
    }
    
    // 便捷方法
    void debug(const std::string& message) { log("DEBUG", message); }
    void info(const std::string& message) { log("INFO", message); }
    void warn(const std::string& message) { log("WARN", message); }
    void error(const std::string& message) { log("ERROR", message); }
    void fatal(const std::string& message) { log("FATAL", message); }
    
    void stop() {
        running_ = false;
        buffer_.stop();
    }
    
private:
    void start_consumer() {
        // 启动消费者协程
        TaskScheduler::getInstance().async_execute([this]() {
            while (running_) {
                try {
                    auto data_future = buffer_.pop_async();
                    auto data = data_future.get();
                    
                    if (data.empty()) continue;
                    
                    // 异步刷盘到所有输出目标
                    std::vector<std::future<void>> flush_futures;
                    for (const auto& flusher : flushers_) {
                        flush_futures.push_back(
                            TaskScheduler::getInstance().async_execute([flusher, data]() {
                                flusher(data);
                            })
                        );
                    }
                    
                    // 等待所有刷盘操作完成
                    for (auto& future : flush_futures) {
                        future.wait();
                    }
                    
                } catch (const std::exception& e) {
                    // 处理异常
                    std::cerr << "Logger error: " << e.what() << std::endl;
                }
            }
        });
    }
    
    std::string format_message(const std::string& level, const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::ostringstream oss;
        oss << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        oss << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";
        oss << "[" << level << "] ";
        oss << "[" << name_ << "] ";
        oss << message << std::endl;
        
        return oss.str();
    }
    
    std::string name_;
    std::vector<FlushFunction> flushers_;
    AsyncBuffer buffer_;
    std::atomic<bool> running_;
};

// 协程风格的日志管理器
class CoroutineLogManager {
public:
    static CoroutineLogManager& getInstance() {
        static CoroutineLogManager instance;
        return instance;
    }
    
    void initialize(size_t thread_count = std::thread::hardware_concurrency()) {
        TaskScheduler::getInstance().start(thread_count);
    }
    
    void shutdown() {
        for (auto& [name, logger] : loggers_) {
            logger->stop();
        }
        TaskScheduler::getInstance().stop();
    }
    
    void add_logger(const std::string& name, 
                   std::shared_ptr<CoroutineStyleAsyncLogger> logger) {
        std::lock_guard<std::mutex> lock(mutex_);
        loggers_[name] = logger;
    }
    
    std::shared_ptr<CoroutineStyleAsyncLogger> get_logger(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = loggers_.find(name);
        return it != loggers_.end() ? it->second : nullptr;
    }
    
private:
    CoroutineLogManager() = default;
    ~CoroutineLogManager() { shutdown(); }
    
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<CoroutineStyleAsyncLogger>> loggers_;
};

} // namespace coroutine_style
} // namespace mylog
