# 协程风格异步日志系统流程详解

## 整体架构流程

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   用户调用      │    │   TaskScheduler │    │   AsyncBuffer   │
│   log_async()   │───▶│   任务调度器    │───▶│   异步缓冲区    │
└─────────────────┘    └─────────────────┘    └─────────────────┘
                                │                       │
                                ▼                       ▼
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   FlushFunction │◀───│   Consumer      │◀───│   Buffer        │
│   输出函数      │    │   消费者协程    │    │   数据交换      │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## 详细流程分析

### 第一阶段：用户调用 (同步入口)
1. **用户调用**: `logger->info("test message")`
2. **路由到**: `log("INFO", "test message")`  
3. **异步转换**: `log_async("INFO", "test message")`
4. **消息格式化**: `format_message()` 生成时间戳和格式化字符串
5. **返回Future**: `buffer_.push_async(formatted)` 返回 `std::future<void>`

### 第二阶段：异步缓冲区处理
1. **任务提交**: `TaskScheduler::async_execute()` 提交写入任务
2. **线程池调度**: 工作线程从任务队列中取出任务
3. **缓冲区写入**: 
   - 检查缓冲区空间
   - 必要时扩展缓冲区
   - 写入格式化数据
   - 通知消费者有新数据

### 第三阶段：消费者协程处理
1. **消费者协程**: `start_consumer()` 中的无限循环
2. **异步读取**: `buffer_.pop_async()` 获取数据
3. **多目标刷盘**: 并行执行所有 `FlushFunction`
4. **等待完成**: 所有刷盘操作完成后继续下一轮

### 第四阶段：实际输出
1. **并行刷盘**: 每个 `FlushFunction` 在独立的任务中执行
2. **文件写入**: 写入到文件、控制台或其他目标
3. **完成通知**: 通过 `promise.set_value()` 通知完成

## 关键技术点

### 1. Future/Promise 异步模式
```cpp
// 每个异步操作都返回future
auto future = logger->log_async("INFO", "message");
// 可以选择等待或不等待
future.wait(); // 等待完成
// 或者不等待，让它在后台执行
```

### 2. 任务调度器的工作原理
```cpp
TaskScheduler::async_execute([this, data]() {
    // 这个lambda会被放入任务队列
    // 工作线程会异步执行这个任务
    // 执行完成后设置promise的值
});
```

### 3. 缓冲区的异步读写
```cpp
// 写入：提交任务到调度器
std::future<void> push_async(const std::string& data) {
    return TaskScheduler::getInstance().async_execute([this, data]() {
        // 实际的写入逻辑在这里异步执行
    });
}

// 读取：同样异步执行
std::future<std::string> pop_async() {
    return TaskScheduler::getInstance().async_execute([this]() -> std::string {
        // 实际的读取逻辑在这里异步执行
    });
}
```

## 与原始实现的对比

### 原始线程池实现流程：
```
用户调用 → 格式化 → AsyncWorker::Push → 条件变量等待 → 缓冲区交换 → RealFlush → 文件写入
```

### 协程风格实现流程：
```
用户调用 → 格式化 → Future<push> → 任务调度 → 异步缓冲 → Future<pop> → 并行刷盘 → 完成通知
```

## 性能优势分析

### 1. 内存效率
- **原始**: 每个线程8MB栈空间
- **协程**: 每个任务几KB空间

### 2. 并发度
- **原始**: 受限于线程池大小
- **协程**: 可以创建大量异步任务

### 3. 调度效率
- **原始**: 重量级线程切换
- **协程**: 轻量级任务切换

## 异常处理流程

```cpp
try {
    func();
    promise->set_value();
} catch (...) {
    promise->set_exception(std::current_exception());
}
```

异常会被捕获并通过promise传播到future，用户可以通过`future.get()`获取异常。

## 生命周期管理

```cpp
// 启动：初始化调度器和工作线程
CoroutineLogManager::getInstance().initialize();

// 运行：处理日志请求
logger->log_async("INFO", "message");

// 关闭：优雅关闭所有组件
CoroutineLogManager::getInstance().shutdown();
```

这种设计确保了资源的正确管理和优雅的关闭。
