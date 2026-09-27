CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter
LDFLAGS  ?= -pthread

SRC = src/main.cpp
BIN = bi

all: $(BIN)

$(BIN): $(SRC) src/*.hpp
	$(CXX) $(CXXFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)

debug: CXXFLAGS = -std=c++17 -g -O0 -Wall -Wextra
debug: clean $(BIN)

clean:
	rm -f $(BIN)

install: $(BIN)
	install -m 755 $(BIN) /usr/local/bin/$(BIN)

.PHONY: all debug clean install