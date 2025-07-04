#pragma once
#include <coroutine>
#include <future>
#include <queue>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace mylog {
namespace coroutine {

// 协程任务类型
template<typename T>
class Task {
public:
    struct promise_type {
        T value;
        std::exception_ptr exception;
        
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        
        void return_value(T val) {
            value = std::move(val);
        }
        
        void unhandled_exception() {
            exception = std::current_exception();
        }
    };
    
    using handle_type = std::coroutine_handle<promise_type>;
    
    Task(handle_type h) : handle_(h) {}
    
    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    
    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    
    T get() {
        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
        return std::move(handle_.promise().value);
    }
    
    bool is_ready() const {
        return handle_.done();
    }
    
private:
    handle_type handle_;
};

// 异步等待器
class AsyncAwaiter {
public:
    AsyncAwaiter(std::chrono::milliseconds delay = std::chrono::milliseconds(1))
        : delay_(delay) {}
    
    bool await_ready() const noexcept { return false; }
    
    void await_suspend(std::coroutine_handle<> h) {
        // 在延迟后恢复协程
        std::thread([h, delay = delay_]() {
            std::this_thread::sleep_for(delay);
            h.resume();
        }).detach();
    }
    
    void await_resume() noexcept {}
    
private:
    std::chrono::milliseconds delay_;
};

// 协程调度器
class CoroutineScheduler {
public:
    static CoroutineScheduler& getInstance() {
        static CoroutineScheduler instance;
        return instance;
    }
    
    template<typename F>
    auto schedule(F&& func) -> Task<void> {
        co_await AsyncAwaiter();
        func();
        co_return;
    }
    
    void yield() {
        return AsyncAwaiter();
    }
    
private:
    CoroutineScheduler() = default;
};

// 协程缓冲区
class CoroutineBuffer {
public:
    CoroutineBuffer(size_t initial_size = 8192) 
        : buffer_(initial_size), write_pos_(0), read_pos_(0) {}
    
    Task<void> push_async(const std::string& data) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 如果缓冲区空间不足，等待消费者消费
        while (write_pos_ + data.size() > buffer_.size()) {
            co_await AsyncAwaiter(std::chrono::milliseconds(1));
            expand_if_needed(data.size());
        }
        
        // 写入数据
        std::copy(data.begin(), data.end(), buffer_.begin() + write_pos_);
        write_pos_ += data.size();
        
        // 通知消费者
        cv_.notify_one();
        co_return;
    }
    
    Task<std::string> pop_async() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 等待数据可用
        while (write_pos_ == read_pos_) {
            cv_.wait(lock);
            co_await AsyncAwaiter(std::chrono::milliseconds(1));
        }
        
        // 读取数据
        std::string result(buffer_.begin() + read_pos_, buffer_.begin() + write_pos_);
        read_pos_ = write_pos_ = 0; // 重置缓冲区
        
        co_return result;
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return write_pos_ == read_pos_;
    }
    
private:
    void expand_if_needed(size_t required_size) {
        if (write_pos_ + required_size > buffer_.size()) {
            buffer_.resize(std::max(buffer_.size() * 2, write_pos_ + required_size));
        }
    }
    
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<char> buffer_;
    size_t write_pos_;
    size_t read_pos_;
};

// 协程版本的异步日志器
class CoroutineAsyncLogger {
public:
    CoroutineAsyncLogger(const std::string& name, 
                        std::vector<std::function<void(const std::string&)>> flushers)
        : name_(name), flushers_(std::move(flushers)), running_(true) {
        // 启动消费者协程
        consumer_task_ = start_consumer();
    }
    
    ~CoroutineAsyncLogger() {
        running_ = false;
        // 这里应该等待协程完成，但为了简化示例代码省略
    }
    
    Task<void> log_async(const std::string& level, const std::string& message) {
        std::string formatted = format_message(level, message);
        co_await buffer_.push_async(formatted);
    }
    
    // 同步接口（内部使用协程）
    void log(const std::string& level, const std::string& message) {
        auto task = log_async(level, message);
        // 在实际实现中，这里应该有更好的方式来处理协程
        // 这里简化为直接调用
    }
    
private:
    Task<void> start_consumer() {
        while (running_) {
            if (!buffer_.empty()) {
                auto data = co_await buffer_.pop_async();
                
                // 异步刷盘
                for (auto& flusher : flushers_) {
                    co_await CoroutineScheduler::getInstance().schedule([&]() {
                        flusher(data);
                    });
                }
            }
            
            // 让出执行权，避免busy waiting
            co_await AsyncAwaiter(std::chrono::milliseconds(10));
        }
    }
    
    std::string format_message(const std::string& level, const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::ostringstream oss;
        oss << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] "
            << "[" << level << "] "
            << "[" << name_ << "] "
            << message << std::endl;
        
        return oss.str();
    }
    
    std::string name_;
    std::vector<std::function<void(const std::string&)>> flushers_;
    CoroutineBuffer buffer_;
    std::atomic<bool> running_;
    Task<void> consumer_task_;
};

} // namespace coroutine
} // namespace mylog
