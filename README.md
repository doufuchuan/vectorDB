# vectorDB
vector database

# v0.0.1
### FaissIndex
系统中对扁平索引对象的封装，隐藏FAISS实现细节
### IndexFactory
向量索引工厂类，进行向量索引生成和获取，支持扁平索引类型的索引
### HttpServer
基于cpp-httplib实现，解析HTTP请求，调用对应索引执行具体操作，支持/insert和/search命令
### GlobalLogger 
基于spdlog实现，提供日志记录能力，提升系统持续运营能力
### VDBServer
main函数，服务的初始化和启动入口

-- 依赖 --
faiss
spdlog
rapidjson

编译得到libxxx.a文件

# References
《从零构建向量数据库》