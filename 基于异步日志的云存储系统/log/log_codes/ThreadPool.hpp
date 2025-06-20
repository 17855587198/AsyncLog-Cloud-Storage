#pragma once
#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool
{
public:
    /*用于初始化线程池，启动指定数量的线程。
    对于每个线程，使用 std::thread 创建一个新线程，并定义线程的执行体。
    在 lambda 函数中，线程会进入一个无限循环，不断尝试从任务队列中获取任务。
    使用 std::unique_lock<std::mutex> 加锁，确保线程安全地访问任务队列。
   调用 condition.wait(lock, [this]{ return this->stop || !this->tasks.empty(); }) 使线程进入等待状态，直到满足以下两个条件之一：线程池停止（stop 为 true）或任务队列不为空。
   如果线程池停止且任务队列为空，线程会退出循环。
    从任务队列中取出一个任务，并将其移动到局部变量 task 中，然后解锁。
     执行取出的任务。
     */
    ThreadPool(size_t threads) // 启动部分线程
        : stop(false)
    {
        for (size_t i = 0; i < threads; ++i)//直接使用可调用函数进行原地构造效率更高，若先单独初始化thread,会多一次临时对象的构造和移动操作
        {
            workers.emplace_back(
                [this]
                {
                    for (;;)
                    {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(this->queue_mutex);
                            // 等待任务队列不为空或线程池停止
                            this->condition.wait(lock,
                                                 [this]
                                                 { return this->stop || !this->tasks.empty(); });
                            if (this->stop && this->tasks.empty())
                                return;
                            //取出任务
                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                        }
                        // 执行任务
                        task();
                    }
                });
        }
    }
    template <class F, class... Args>
    // 该函数用于将一个新任务添加到任务队列中，并返回一个 std::future 对象，用于获取任务的执行结果。
    // 使用 std::packaged_task 将传入的函数 f 和参数 args 打包成一个可调用对象。
    // 通过 task->get_future() 获取一个 std::future 对象，用于异步获取任务的返回值。
    // 使用 std::unique_lock<std::mutex> 加锁，确保线程安全地访问任务队列。
    // 检查线程池是否已经停止，如果停止则抛出异常。
    // 将打包好的任务封装成一个 lambda 函数，并添加到任务队列中。
    // 调用 condition.notify_one() 唤醒一个等待的线程，通知它有新任务可用。
    // 返回 std::future 对象。
    auto enqueue(F &&f, Args &&...args)
        -> std::future<typename std::result_of<F(Args...)>::type>//接受任意任务及其参数，返回future对象用于捕获异步结果
    {
        /*
        // 错误写法：编译时不知道 F 和 Args 是什么，无法直接写返回类型
        template<typename F, typename... Args>
          std::result_of<F(Args...)>::type enqueue(F&& f, Args&&... args) { ... }
          尾置返回类型允许在参数列表之后声明返回类型，此时 F 和 Args 已经是已知的。
        */
        using return_type = typename std::result_of<F(Args...)>::type;//确定返回值类型

        // 创建一个打包任务
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));//完美转发避免不必要拷贝==>返回值是return_type,无参数

        /*
        // 假设 F 是 int foo(int, double)，args 是 (42, 3.14)
                auto task = std::make_shared<std::packaged_task<int()>>(
                    []() { return foo(42, 3.14); }  // 绑定参数后的 Lambda
                );
        */

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            if (stop)// 如果线程池已停止，抛出异常
                throw std::runtime_error("enqueue on stopped ThreadPool");
            // 将任务添加到任务队列
            tasks.emplace([task]()
                          { (*task)(); });
        }
        condition.notify_one();
        return res;
    }
    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker : workers)
        {
            worker.join();//加入并执行
        }
    }

private:
    std::vector<std::thread> workers;        // 线程们
    std::queue<std::function<void()>> tasks; // 任务队列
    std::mutex queue_mutex;                  // 任务队列的互斥锁
    std::condition_variable condition;       // 条件变量，用于任务队列的同步
    bool stop;
};