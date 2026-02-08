CXX = g++
CXXFLAGS = -std=c++14 -g -Wall -pthread $(patsubst %, -I%, $(shell find ./src -type d))
LDFLAGS = -pthread -lmysqlclient

TARGET = ./bin/server
SRC = $(shell find ./src -name "*.cpp")

$(TARGET): $(SRC)
	@mkdir -p ./bin
	$(CXX) $(CXXFLAGS)  $(SRC) $(LDFLAGS) -o $(TARGET)

.PHONY: clean run
clean:
	rm -rf ./bin

run: $(TARGET)
	$(TARGET)