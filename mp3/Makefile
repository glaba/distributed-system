INC_DIR        := inc
CXX            := g++
CXXFLAGS       := -I$(INC_DIR) -std=c++17 -Wall -DDEBUG -O3 -g
LDFLAGS        := -pthread

OBJ_DIR        := obj
SRC_DIR        := src
TEST_DIR       := tests

SRC_FILES      := $(wildcard $(SRC_DIR)/*.cpp)
TEST_FILES     := $(wildcard $(TEST_DIR)/*.cpp)
SRC_OBJ_FILES  := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC_FILES))
TEST_OBJ_FILES := $(patsubst $(TEST_DIR)/%.cpp,$(OBJ_DIR)/%.test.o,$(TEST_FILES))
OBJ_FILES      := $(SRC_OBJ_FILES) $(TEST_OBJ_FILES)

MEMBER         := member
ELECTION       := election

all: $(MEMBER) #$(ELECTION)

$(MEMBER): $(filter-out obj/sdfs.o obj/sdfs_client.o obj/sdfs_server.o, $(OBJ_FILES))
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(ELECTION): $(filter-out obj/member.o, $(OBJ_FILES))
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJ_DIR)/%.test.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

.PHONY: all clean

clean:
	rm member election obj/*
