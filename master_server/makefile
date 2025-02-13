CXX = g++
CXXFLAGS = -Wall -std=c++11 -I/usr/local/include -I ../include -I ../third_party/rapidjson/include -I ../third_party/spdlog/include
LDFLAGS = -lcurl -lpthread -letcd-cpp-api -lcpprest -Wl,-rpath,/usr/local/lib64 
# 源文件
SOURCES = master_server.cpp vdb_server_master.cpp logger.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# 可执行文件
TARGET = vdb_server_master

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all clean