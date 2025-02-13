CXX = g++ # 使用 g++ 编译器
CXXFLAGS = -Wall -std=c++11 -I ../include -I ../third_party/rapidjson/include -I ../third_party/spdlog/include# 编译标志，启用所有警告和C++11标准
LDFLAGS = -lcurl -lpthread -lspdlog # 链接标志，链接 libcurl, pthread 和 spdlog

# 源文件列表
SOURCES = proxy_server.cpp vdb_server_proxy.cpp logger.cpp
HEADERS = proxy_server.h logger.h
OBJECTS = $(SOURCES:.cpp=.o)

# 目标二进制文件
TARGET = vdb_server_proxy

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all clean
