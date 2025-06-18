#pragma once
#include<atomic>
#include<cassert>
#include<cstdarg>//va_list数据结构
#include<memory>
#include<mutex>
#include "Level.hpp"
#include "AsyncWorker.hpp"
#include "Message.hpp"
#include "LogFlush.hpp"
#include "backlog/clientBackupLog.hpp"
#include "ThreadPool.hpp"
/*----------将组织好的日志放入缓冲区---------*/
