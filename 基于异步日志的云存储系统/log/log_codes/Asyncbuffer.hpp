//设计日志的缓冲区类
#pragma once
#include<cassert>
#include<string>
#include<vector>
#include "Util.hpp"
/*
(1)环形缓冲区变种：通过 read_pos_ 和 write_pos_ 标记读写位置，实现高效数据流转。
(2)动态扩容策略：根据配置参数（g_conf_data）自动调整缓冲区大小，平衡内存占用与性能。
(3)线程安全性：代码本身未加锁，在外部（生产者-消费者模型中使用互斥锁）保证了多线程安全。
*/

extern mylog::Util::JsonData *g_conf_data;

namespace mylog{
    class Buffer{
        public:
          Buffer():write_pos_(0),read_pos_(0)//初始的缓冲区大小由配置文件决定
          {
            buffer_.resize(g_conf_data->buffer_size);
          }

          void Push(const char *data,size_t len)//生产者
          {
             ToBeEnough(len); // 确保容量足够
            // 开始写入
            std::copy(data, data + len, &buffer_[write_pos_]);//将从data开始的长度为len的数据写入buffer_中
            write_pos_ += len;//更新写的位置
            
          }

          char *ReadBegin(int len)
          {
             assert(len <= ReadableSize());
             return &buffer_[read_pos_];
          }

          bool IsEmpty()
          {
            return write_pos_==read_pos_;//判断是否为空
          }

          void Swap(Buffer &buf)//直接交换指针，减少磁盘IO
          {
             buffer_.swap(buf.buffer_);//直接交换两个vector底层数据(指针,大小，容量等),不涉及数据的拷贝
             std::swap(read_pos_,buf.read_pos_);
             std::swap(write_pos_,buf.write_pos_);
          }

          size_t WriteableSize(){
            return buffer_.size()-write_pos_;//写空间剩余的容量
          }

          size_t ReadableSize()
          {
            return write_pos_-read_pos_;//读空间剩余的容量
          }

          const char *Begin(){
            return &buffer_[read_pos_];//返回开始读取位置的指针
          }

          void MoveWritePos(int len)//已经向缓冲区写入len字节的数据，更新可写空间
          {
              assert(len<=WriteableSize());//确保移动的字节数不超过当前可写空间,否则终止
              write_pos_+=len;
          }

          void MoveReadPos(int len)//移动读位置指针
          {
            assert(len<=ReadableSize());
            read_pos_+=len;
          }

          void Reset(){//重置
            write_pos_=0;
            read_pos_=0;
          }

        protected:

           void ToBeEnough(size_t len)
           {
            int buffersize=buffer_.size();
            if(len>=WriteableSize())//容量不足，扩容
            {
              if(buffer_.size()<g_conf_data->threshold)
              {
                buffer_.resize(2*buffer_.size()+buffersize);//成倍扩容

              }
              else{
                buffer_.resize(g_conf_data->linear_growth+buffersize);//线性扩容
              }
            }
           }
           std::vector<char>buffer_;
           size_t write_pos_;
           size_t read_pos_;

    };

}