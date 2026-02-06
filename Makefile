CXX = g++
CXXFLAGS = -std=c++14 -g -Wall -pthread -I./http_conn -I./web_server -I./sql_pool
LDFLAGS = -pthread -lmysqlclient

TARGET = server
SRC = $(wildcard *.cpp ./http_conn/*.cpp ./web_server/*.cpp ./sql_pool/*.cpp)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS)  $(SRC) $(LDFLAGS) -o $(TARGET)

.PHONY: clean run
clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)