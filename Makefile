CXX = g++
CXXFLAGS = -std=c++14 -g -Wall -pthread -I./http_conn -I./web_server -I./sql_pool -MMD
LDFLAGS = -pthread -lmysqlclient

TARGET = myserver
SRC = $(wildcard *.cpp ./http_conn/*.cpp ./web_server/*.cpp ./sql_pool/*.cpp)
OBJ = $(patsubst %.cpp,%.o,$(SRC))
DEPS = $(OBJ:.o=.d)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS)  $(OBJ) $(LDFLAGS) -o $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

.PHONY: clean
clean:
	rm -f $(OBJ) $(TARGET) $(DEPS)