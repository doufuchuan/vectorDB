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

# 运行

```
./vdb_server
```

插入
```
curl -X POST -H "Content-Type: application/json" -d '{"vectors": [0.8], "id": 2, "indexType": "FLAT"}' http://localhost:8080/insert
```

查询
```
curl -X POST -H "Content-Type: application/json" -d '{"vectors": [0.8], "k": 2, "indexType": "FLAT"}' http://localhost:8080/search
```
返回结果：
```
{"vectors":[2],"distances":[0.09000000357627869],"retCode":0}
```

# v0.0.2
实现HNSW索引封装

依赖：hnswlib

插入
```
curl -X POST -H "Content-Type: application/json" -d '{"vectors": [0.2], "id": 3, "indexType": "HNSW"}' http://localhost:8080/insert
```

查询
```
curl -X POST -H "Content-Type: application/json" -d '{"vectors": [0.5], "k": 2, "indexType": "HNSW"}' http://localhost:8080/search
```

返回结果
```
{"vectors":[3],"distances":[0.09000000357627869],"retCode":0}
```

# v0.1
混合索引
- 基于RocksDB实现标量数据索引
    - `ScalarStorage`类
- `VectorDataBase`统一管理入口，组合向量数据和标量数据操作
- 扩展`upsert`、`query`接口

```
curl -X POST -H "Content-Type: application/json" -d '{"vectors": [0.2], "id": 3, "indexType": "FLAT", "Name": "hello", "Ci":1111}' http://localhost:8080/upsert
```

```
curl -X POST -H "Content-Type: application" -d {"id": 3}' http://localhost:8080/query
```


# References
《从零构建向量数据库》