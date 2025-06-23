#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include "DataManage.hpp"
#include <string>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

using std::cout;
using std::endl;
using std::cerr;

const unsigned short server_port_g = 8081;
const std::string server_ip_g = "127.0.0.1";//就选用本地进行存储
const std::string storage_filename_g = "./storage.dat";//用于存储上传的文件信息

namespace my_storage{
	bool if_upload_success=false;//标识是否成功上传
	
	class Storage{
		private:
		   DataManage *dm_;
		   std::string low_storage_dir_;//浅存储
		   std::string deep_storage_dir_;//深存储
		
		public:
		/*构造函数*/
		  Storage(const std::string& low_storage_dir, const std::string& deep_storage_dir)
			:low_storage_dir_(low_storage_dir), deep_storage_dir_(deep_storage_dir) {
			dm_ = new DataManager(storage_filename_g);
		}

		/*文件标识生成，即通过文件名，文件大小以及最后修改时间生成唯一的文件标识，用于判断文件是否需要重新上传*/
		std::string GetFileIdentifier(const std::string& filename) {
			FileUtil fu(filename);
			std::stringstream ss;//filenam.txt-16-2025.6.13
			ss << fu.FileName() << "-" << fu.FileSize() << "-" << fu.LastModifyTime();
			return ss.str();
		}
        /*当 HTTP 上传请求完成（成功或失败）时被触发*/
		static void upload_callback(struct evhttp_request* req, void* arg)//上传的回调函数,在 libevent 的 HTTP 回调函数中，evhttp_request* req 参数是 输入参数（只读），表示 服务器返回的 HTTP 响应
		{
			cout << "http_client_cb" << endl;
			event_base* bev=(event_base*)arg;//获取事件的基础对象(arg 是用户传入的上下文参数，这里传递了 event_base 对象，用于控制事件循环)
			if(req==nullptr)//检查请求对象的有效性
			{
				int errcode = EVUTIL_SOCKET_ERROR();
				cerr << "socket error " << evutil_socket_error_to_string(errcode) << endl;
				return;
			}
			int rsp_code=evhttp_request_get_response_code(req);//获取http响应的状态码
			if(rsp_code!=HTTP_OK)//处理响应结果
			{
               cerr << "response:" << rsp_code << endl;
			   if_upload_success = false;
			}
			else{
				if_upload_success=true;
			}
            event_base_loopbreak(bev);//终止事件循环

		}
        /*
		文件上传,即从本地上传到服务端:
		(1)使用libevent库实现异步HTTP上传;
		(2)支持大文件传输;
		(3)自动添加文件元信息到请求头
		客户端向服务器发起文件上传请求:发送 POST 请求到服务器的 /upload 路径
		*/
		bool Upload(const std::string &filename)
		{
         FileUtil fu(filename);//文件工具类
		 event_base *base=event_base_new();//初始化事件基础,创建底座,创建事件处理核心
		 if (base == NULL) {
				cerr << __FILE__ << __LINE__ << "event_base_new() err" << endl;
				event_base_free(base);
				return false;
			}
		/*创建连接组件:bufferevent连接http服务器*/
		bufferevent* bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);//创建缓冲区事件
		evhttp_connection* evcon = evhttp_connection_base_bufferevent_new(base, NULL, bev, server_ip_g.c_str(), server_port_g);//基于bufferevent创建http连接对象
        /*创建HTTP请求并设置回调,当服务器响应时触发回调函数upload_back*/
		evhttp_request* req = evhttp_request_new(upload_callback, base);
//设置请求头
	    evkeyvalq* output_headers = evhttp_request_get_output_headers(req);
		evhttp_add_header(output_headers, "Host", server_ip_g.c_str());//必要的头
		evhttp_add_header(output_headers, "FileName", fu.FileName().c_str());//自定义头
		evhttp_add_header(output_headers, "Content-Type", "application/octet-stream");//二进制类型
		if (filename.find("low_storage") != std::string::npos) {
				evhttp_add_header(output_headers, "StorageType", "low");
			}//根据文件名设置存储类型
		else {
				evhttp_add_header(output_headers, "StorageType", "deep");
			}
/*读取文件内容添加到请求体*/
		std::string body;
		fu.GetContent(&body);// 将文件内容读入body
		evbuffer* output = evhttp_request_get_output_buffer(req);
		/*evbuffer_add(output, body.c_str(), body.size())，将文件内容写入请求体*/
		if (0 != evbuffer_add(output, body.c_str(), body.size())) {
			cerr << __FILE__ << __LINE__ << "evbuffer_add_printf() err" << endl;
			}

         /*客户端发起请求*/
		if (0 != evhttp_make_request(evcon, req, EVHTTP_REQ_POST, "/upload")) {
			cerr << __FILE__ << __LINE__ << "evhttp_make_request() err" << endl;
			}//发送Http请求
         //进入事件循环等待响应
		if (base)
			event_base_dispatch(base); // 阻塞直到回调调用event_base_loopbreak
		if (base)
			event_base_free(base);
		return true;

		}
	/*文件变化检测:避免重复上传未修改文件,防止上传正在被编辑的文件*/
		bool IfNeedUpload(const std::string& filename) {
			std::string old;
			if (dm_->GetOneByKey(filename, &old)==true) {//true indicates that the storage file info already exists
				if (GetFileIdentifier(filename) == old)
					return false;
			}

			// 在文件未修改时上传文件
			FileUtil fu(filename);
			if (time(NULL) - fu.LastModifyTime() < 5) {
				// just changed in 5 seconds，think the file is still being changed
				return false;
			}
			return true;
		}
		/*文件上传服务器*/
		bool RunModule() {
			while (1) {
				//iterate to get all files in the specified folder,and judge need upload or not
				FileUtil low(low_storage_dir_);
				FileUtil deep(deep_storage_dir_);
				std::vector<std::string> arry;//主要是关于不同的arry
				low.ScanDirectory(&arry);
				deep.ScanDirectory(&arry);
				//2. upload
				for (auto& a : arry) {
					if (IfNeedUpload(a) == false) {
						continue;
					}
					Upload(a);
					if (if_upload_success == true) {
						dm_->Insert(a, GetFileIdentifier(a));
						std::cout << a << " upload success!\n";
					}
					else {
						std::cout << a << " upload failed!\n";
					}
				}
				Sleep(1);//avoid waste cpu 
			}
		}
	};
}