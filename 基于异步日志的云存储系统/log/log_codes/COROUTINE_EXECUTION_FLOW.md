## 协程风格异步日志系统完整执行流程

### 流程概述

经过修改后的协程风格异步日志系统采用了 **Future/Promise + 任务调度器** 的设计模式，相比原始的线程池+双缓冲区方案，具有更好的性能和可扩展性。

### 详细执行流程

#### 1. 系统初始化阶段
```cpp
// 用户代码
CoroutineLogManager::getInstance().initialize(4);

// 内部执行流程：
TaskScheduler::start(4) 
  → 创建4个工作线程
  → 每个线程监听任务队列
  → 系统就绪，等待任务
```

#### 2. 日志器创建阶段
```cpp
// 用户代码
auto logger = std::make_shared<CoroutineStyleAsyncLogger>("demo", flushers);

// 内部执行流程：
构造函数调用 
  → 保存name和flushers
  → 创建AsyncBuffer实例
  → 调用start_consumer()
  → 提交消费者协程到TaskScheduler
  → 消费者协程开始在后台运行
```

#### 3. 日志写入阶段（核心流程）
```cpp
// 用户代码
auto future = logger->log_async("INFO", "test message");

// 内部执行流程：
log_async()
  ↓
format_message() → 生成格式化的日志字符串
  ↓
buffer_.push_async(formatted) → 返回std::future<void>
  ↓
TaskScheduler::async_execute() → 提交写入任务
  ↓
工作线程执行写入任务:
  - 获取缓冲区锁
  - 检查/扩展缓冲区空间
  - 写入数据到缓冲区
  - 通知消费者协程
  - 设置promise完成状态
```

#### 4. 消费者协程处理阶段
```cpp
// start_consumer() 中的无限循环
while (running_) {
    auto data_future = buffer_.pop_async();  // 异步读取
    auto data = data_future.get();           // 等待数据
    
    // 并行刷盘到所有目标
    std::vector<std::future<void>> flush_futures;
    for (const auto& flusher : flushers_) {
        flush_futures.push_back(
            TaskScheduler::async_execute([flusher, data]() {
                flusher(data);  // 实际的文件/控制台输出
            })
        );
    }
    
    // 等待所有刷盘完成
    for (auto& future : flush_futures) {
        future.wait();
    }
}
```

#### 5. 实际输出阶段
```cpp
// FlushFunction 在独立的任务中执行
TaskScheduler::async_execute([flusher, data]() {
    flusher(data);  // 例如：写入文件、输出到控制台
});
```

### 关键优化点

#### 1. 异步化所有操作
- **日志写入**: `push_async()` 返回 `future<void>`
- **数据读取**: `pop_async()` 返回 `future<string>`
- **数据刷盘**: 每个 `flusher` 都在独立任务中执行

#### 2. 并行处理
- 多个 `FlushFunction` 并行执行
- 缓冲区读写并行进行
- 用户线程不会被阻塞

#### 3. 资源效率
- 任务比线程更轻量级
- 动态任务创建，无固定开销
- 更好的CPU利用率

### 性能特点

#### 与原始实现对比

| 特性 | 原始线程池实现 | 协程风格实现 |
|------|----------------|--------------|
| 并发模型 | 生产者-消费者+线程池 | Future/Promise+任务调度 |
| 内存使用 | 每线程8MB | 每任务几KB |
| 扩展性 | 受限于线程数 | 可创建大量任务 |
| 延迟 | 线程切换开销 | 轻量级任务切换 |
| 复杂度 | 复杂的同步机制 | 简洁的异步模型 |

#### 性能提升点

1. **写入性能**: 
   - 用户调用立即返回future
   - 实际写入在后台异步进行
   - 无阻塞等待

2. **刷盘性能**:
   - 多个输出目标并行处理
   - 每个刷盘操作独立执行
   - 不相互阻塞

3. **系统吞吐量**:
   - 可以处理更多并发请求
   - 更好的CPU利用率
   - 更少的资源争用

### 使用示例

```cpp
// 初始化
CoroutineLogManager::getInstance().initialize();

// 创建日志器
auto logger = std::make_shared<CoroutineStyleAsyncLogger>("app", flushers);

// 高性能日志写入
std::vector<std::future<void>> futures;
for (int i = 0; i < 10000; ++i) {
    futures.push_back(logger->log_async("INFO", "Message " + std::to_string(i)));
}

// 等待所有日志完成（可选）
for (auto& future : futures) {
    future.wait();
}

// 关闭系统
CoroutineLogManager::getInstance().shutdown();
```

这种设计使得异步日志系统具有了更好的性能、可维护性和扩展性。
