INC_DIR   := inc
CXX       := g++
CXXFLAGS  := -I$(INC_DIR) -std=c++14 -Wall -DDEBUG -g
LDFLAGS   := -pthread

OBJ_DIR   := obj
SRC_DIR   := src

SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC_FILES))

MEMBER    := member
ELECTION  := election

all: $(MEMBER) $(ELECTION)

$(MEMBER): $(filter-out obj/sdfs.o, $(OBJ_FILES))
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(ELECTION): $(filter-out obj/member.o, $(OBJ_FILES))
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

.PHONY: all clean

clean:
	rm member obj/*
