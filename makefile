# 编译器
CXX = g++

PWD := $(shell pwd)

# 编译选项
CXXFLAGS = -std=c++17 -g -Wall $(INCLUDES)

LIBS = -L 

# 链接选项
LDFLAGS = ./third_party/lib/libfaiss.a -fopenmp -lopenblas -lpthread ./third_party/lib/libspdlog.a -lrocksdb

INCLUDES = -I $(PWD)/include -I ./third_party/faiss -I ./third_party/rapidjson/include -I ./third_party/spdlog/include -I /usr/local/include -I ./third_party/hnswlib -I ./third_party/rocksdb/include

# 目标文件
TARGET = vdb_server

# 源文件
SOURCES = $(wildcard ./src/*.cpp)#vdb_server.cpp faiss_index.cpp http_server.cpp index_factory.cpp logger.cpp

# 对象文件
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)