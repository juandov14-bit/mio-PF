CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -I../ -I.

SRC = \
  ../API_Control_PID.cpp \
  ../API_MyTimer.cpp \
  ../API_Resistor.cpp \
  ../API_Sensors.cpp \
  test_main.cpp

INCLUDES = -I../ -I./mocks

BIN = build/test_bin

all: $(BIN)

$(BIN): $(SRC)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SRC)

run: $(BIN)
	./$(BIN)

clean:
	rm -rf build

.PHONY: all run clean

