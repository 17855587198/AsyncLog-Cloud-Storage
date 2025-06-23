#pragma once
#include<unordered_map>
#include "Util.hpp"
/*
(1)提供键值对的插入、更新、查询功能；
(2)支持数据持久化存储到本地；
(3)支持从文件初始化加载到内存；
*/
namespace my_storage{
    class DataManage{

        public:
          DataManage(const std::string storage_file):storage_file_(storage_file)
          {
            InitLoad();//初始化加载
          }
          /*格式：文件路径+" "+文件的存储信息,即问文件的存储信息(按照空格解析字符串)*/
           void Split(const std::string& body, const std::string& sep, std::vector<std::string>* arr) {	
			size_t pos = 0;
			size_t begin = 0;
			while (begin < body.size()) {
				pos = body.find(sep, begin);//sep是需要找到的分割线
				if (pos < body.size()) {
					//Indicates that there is a file storage info
					arr->emplace_back(body.begin() + begin, body.begin() + pos);
					begin = pos + 1;//begin会更新为找到sep的下一个index
				}
				else {
					arr->emplace_back(body.begin() + begin, body.end());
					begin = body.size();
				}
			}
		}
        /*初始化加载（InitLoad），主要用于从存储文件（storage_file_）
        中读取之前保存的数据，并将其解析为键值对（key-value）存储到内存中的 
        table_*/
        bool InitLoad() {//初始化加载
			FileUtil fu(storage_file_);
			if (fu.Exists() == false) return true;//判断存储文件是否存在
			std::string content;
			fu.GetContent(&content);
			//解析文件内容，获取文件信息
			std::vector<std::string> arry;
			Split(content, "\n", &arry);// 用 Split(content, "\n", &arry) 按换行符 \n 分割文件内容，每行代表一个键值对记录
			for (auto& e : arry) {//对于每一行调用一次分割函数
				std::vector<std::string> s;
				Split(e, " ", &s);//对每个文件按照" "进行对key-value方式划分 
				table_[s[0]] = s[1];//s[0] 存储 key ,s[1] 存储 value
			}
			return true;
		}

        bool Storage() {//存储文件
			//将table_中的信息加载进存储信息 
			std::stringstream ss;
			for (auto e : table_) {
				ss << e.first << " " << e.second << "\n";//格式file name + 空格 + unique identifier
			}
			FileUtil fu(storage_file_);
			fu.SetContent(ss.str());
			return true;
		}
        bool Insert(const std::string& key, const std::string& val) {
			table_[key] = val;
			Storage();
			return true;
		}

		bool Update(const std::string& key, const std::string& val) //修改后能同步到存储文件
		{
			table_[key] = val;
			Storage();
			return true;
		}
		bool GetOneByKey(const std::string& key, std::string* val)//是否能通过文件路径找到
        {
			auto it = table_.find(key);
			if (it != table_.end()) {
				*val = it->second;
				return true;
			}
			return false;
		}
        
        private:
          std::string storage_file_;//存储的文件(需要存储的文件)
           /*key代表文件路径，value代表文件的独特存储信息*/
           std::unordered_map<std::string,std::string>table_;


    };
}