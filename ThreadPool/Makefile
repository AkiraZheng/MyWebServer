CXX = g++
TARGET = test
# 自动实现把当前目录下的所有.cpp文件转换成.o文件
SRC = $(wildcard *.cpp)
# SRC += $(wildcard timer/*.cpp)# 添加timer文件夹里的文件
OBJS = $(patsubst %.cpp, %.o, $(SRC))

# 编译选项 -c 表示编译链接分开进行 -Wall 表示显示所有警告信息
CXXFLAGS = -c -Wall

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^

# 简化所有的.o文件的生成规则
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f *.o $(TARGET)