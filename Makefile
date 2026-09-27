CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter
LDFLAGS  ?=
LDLIBS   ?= -pthread

# Enable fetch() if libcurl is available.
# Comment these two lines out if you don't want curl:
CXXFLAGS += -DBI_HAVE_CURL
LDLIBS   += -lcurl

SRC      := src/main.cpp
BIN      := bi

.PHONY: all clean test run

all: $(BIN)

$(BIN): $(SRC) src/*.hpp
	$(CXX) $(CXXFLAGS) $(SRC) -o $(BIN) $(LDFLAGS) $(LDLIBS)

test: $(BIN)
	./$(BIN) test

clean:
	rm -f $(BIN)