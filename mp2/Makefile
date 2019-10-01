INC_DIR			:= inc
CXX 				:= -g++
CXXFLAGS		:= -I$(INC_DIR) -std=c++11 -Wall -DDEBUG -g

OBJ_DIR			:= obj
SRC_DIR			:= src

UTILS_SRC 	:= utils.cpp
MEMBER_SRC	:= member.cpp

UTILS_OBJ		:= $(OBJ_DIR)/$(UTILS_SRC:.cpp=.o)
MEMBER_OBJ	:= $(OBJ_DIR)/$(MEMBER_SRC:.cpp=.o)

MEMBER			:= member

all: $(MEMBER)

$(MEMBER): $(MEMBER_OBJ) $(UTILS_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

.PHONY: all clean client server

clean:
	rm member obj/*
