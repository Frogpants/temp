CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic
TARGET := app
RG_TARGET := ragdoll_viewer
SRC := main.cpp
RG_SRC := ragdoll_viewer.cpp

.PHONY: all run rg clean

all: run

$(TARGET): $(SRC) neuron.hpp essentials.hpp
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

$(RG_TARGET): $(RG_SRC) body.hpp ragdoll.hpp renderer.hpp neuron.hpp essentials.hpp
	$(CXX) $(CXXFLAGS) $(RG_SRC) -o $(RG_TARGET) -lglfw -lGL -lGLU -ldl -lpthread

rg: $(RG_TARGET)
	./$(RG_TARGET)
