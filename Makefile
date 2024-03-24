# ## Version 1
# test: main.cpp config.cpp
# 	g++ -o test main.cpp config.cpp

# ## Version 2
# CXX = g++
# TARGET = test
# OBJS = main.o config.o

# $(TARGET): $(OBJS)
# 	$(CXX) -o $(TARGET) $(OBJS)

# main.o: main.cpp
# 	$(CXX) -c main.cpp

# config.o: config.cpp
# 	$(CXX) -c config.cpp

# ## Version 3
# CXX = g++
# TARGET = test
# OBJS = main.o config.o webserver.o

# ## 编译选项 -c 表示编译链接分开进行 -Wall 表示显示所有警告信息
# CXXFLAGS = -c -Wall

# $(TARGET): $(OBJS)
# 	$(CXX) -o $@ $^

# # 简化所有的.o文件的生成规则
# %.o: %.cpp
# 	$(CXX) $(CXXFLAGS) $< -o $@

# .PHONY: clean
# clean:
# 	rm -f *.o $(TARGET)

# ## Version 4
# CXX = g++
# TARGET = test
# # 自动实现把当前目录下的所有.cpp文件转换成.o文件
# # 添加timer文件夹里的文件

# SRC = $(wildcard *.cpp)
# OBJS = $(patsubst %.cpp, %.o, $(SRC))

# # 编译选项 -c 表示编译链接分开进行 -Wall 表示显示所有警告信息
# CXXFLAGS = -c -Wall

# $(TARGET): $(OBJS)
# 	$(CXX) -o $@ $^

# # 简化所有的.o文件的生成规则
# %.o: %.cpp
# 	$(CXX) $(CXXFLAGS) $< -o $@

# .PHONY: clean
# clean:
# 	rm -f *.o $(TARGET)

## version 5
# CXX = g++
# TARGET = server
# # 自动实现把当前目录下的所有.cpp文件转换成.o文件
# SRC = $(wildcard *.cpp)
# SRC += $(wildcard timer/*.cpp)# 添加timer文件夹里的文件
# SRC += $(wildcard http/*.cpp)
# SRC += $(wildcard log/*.cpp)
# SRC += $(wildcard CGImysql/*.cpp)
# OBJS = $(patsubst %.cpp, %.o, $(SRC))

# # 编译选项 -c 表示编译链接分开进行 -Wall 表示显示所有警告信息
# CXXFLAGS = -c -Wall

# $(TARGET): $(OBJS)
# 	$(CXX) -o $@ $^

# # 简化所有的.o文件的生成规则
# %.o: %.cpp
# 	$(CXX) $(CXXFLAGS) $< -o $@

# .PHONY: clean
# clean:
# 	rm -f *.o $(TARGET)

CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

server: main.cpp  ./timer/lst_timer.cpp ./http/http_conn.cpp ./log/log.cpp ./CGImysql/sql_connection_pool.cpp  webserver.cpp config.cpp
	$(CXX) -o server  $^ $(CXXFLAGS) -lpthread -lmysqlclient

clean:
	rm  -r server