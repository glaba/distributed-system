INC_DIR			:= inc
CXX 				:= -g++
CXXFLAGS		:= -I$(INC_DIR) -std=c++11 -Wall -DDEBUG -g
LDFLAGS			:=

OBJ_DIR			:= obj
SRC_DIR			:= src

SERVER_SRC	:= server.cpp
CLIENT_SRC	:= client.cpp
SHARED_SRC 	:= utils.cpp

SERVER_OBJ	:= $(OBJ_DIR)/$(SERVER_SRC:.cpp=.o)
CLIENT_OBJ	:= $(OBJ_DIR)/$(CLIENT_SRC:.cpp=.o)
SHARED_OBJ	:= $(OBJ_DIR)/$(SHARED_SRC:.cpp=.o)

CLIENT			:= client
SERVER			:= server

SCRIPTS_CLIENT := ../.scripts/dgrep-client
SCRIPTS_SERVER := ../.scripts/dgrep-server

all: $(CLIENT) $(SERVER)

$(CLIENT): $(CLIENT_OBJ) $(SHARED_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@
	@cp $(CLIENT) $(SCRIPTS_CLIENT)

$(SERVER): $(SERVER_OBJ) $(SHARED_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@
	@cp $(SERVER) $(SCRIPTS_SERVER)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

.PHONY: all clean client server

clean:
	rm client server obj/*
