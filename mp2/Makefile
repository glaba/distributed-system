INC_DIR   := inc
CXX       := -g++
CXXFLAGS  := -I$(INC_DIR) -std=c++11 -Wall -DDEBUG -g
LDFLAGS   := -pthread

OBJ_DIR   := obj
SRC_DIR   := src

SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC_FILES))

MEMBER    := member

all: $(MEMBER)

$(MEMBER): $(OBJ_FILES)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

.PHONY: all clean

clean:
	rm member obj/*
