# 定义编译器
CXX = g++

# 编译选项
CXXFLAGS = -Wall  # 开启警告
LDFLAGS = -lgflags -lpthread  # 链接 gflags 和 pthread

# 目标文件
TARGET = bin/main
SRC = main.cpp
OBJ = $(SRC:.cpp=.o)

# 生成可执行文件
$(TARGET): $(OBJ) | bin
	$(CXX) -o $@ $^ $(LDFLAGS)

# 生成 .o 文件
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 创建 bin 目录
bin:
	mkdir -p bin

run: $(TARGET)
	./$(TARGET)

# 清理编译文件
clean:
	rm -f $(OBJ) $(TARGET)