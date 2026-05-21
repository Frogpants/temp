CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic
TARGET := app
SRC := main.cpp

.PHONY: all run clean

all: run

$(TARGET): $(SRC) neuron.hpp essentials.hpp
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
