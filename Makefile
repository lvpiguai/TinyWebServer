CXX = g++
CXXFLAGS = -std=c++14 -g -Wall -pthread -I./src/http_conn -I./src/web_server -I./src/sql_pool -I./src/timer -I./src/log
LDFLAGS = -pthread -lmysqlclient

TARGET = server
SRC = $(shell find ./src -name "*.cpp")

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS)  $(SRC) $(LDFLAGS) -o $(TARGET)

.PHONY: clean run
clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)