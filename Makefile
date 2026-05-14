CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic -pthread
TARGET := custom_htop
SOURCES := main.cpp $(wildcard src/*.cpp)
HEADERS := $(wildcard include/*.h)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -Iinclude -o $@ $(SOURCES)

run: $(TARGET)
	./$(TARGET) 8080

clean:
	rm -f $(TARGET)
