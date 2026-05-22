CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic
LDLIBS := -lglfw -lGL -ldl -lpthread
TARGET := win
SRC := main.cpp

.PHONY: all run rg clean

all: run

$(TARGET): $(SRC) window.hpp
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDLIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

