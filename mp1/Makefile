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

SCRIPTS_DIR					:= ../.scripts
SCRIPTS_CLIENT 			:= dgrep-client
SCRIPTS_SERVER 			:= dgrep-server
SCRIPTS_DGREP				:= dgrep.sh
SCRIPTS_DGREP_SETUP	:= dgrep_setup.sh

SCRIPTS_DGREP_DEST				:= dgrep
SCRIPTS_DGREP_SETUP_DEST	:= dgrep_setup

COPY_SCRIPTS				:= $(SCRIPTS_DIR)/$(SCRIPTS_CLIENT) $(SCRIPTS_DIR)/$(SCRIPTS_SERVER) $(SCRIPTS_DIR)/$(SCRIPTS_DGREP_DEST) $(SCRIPTS_DIR)/$(SCRIPTS_DGREP_SETUP_DEST)


all: $(CLIENT) $(SERVER) $(COPY_SCRIPTS)

$(CLIENT): $(CLIENT_OBJ) $(SHARED_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(SERVER): $(SERVER_OBJ) $(SHARED_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(SCRIPTS_DIR)/$(SCRIPTS_CLIENT): $(SCRIPTS_CLIENT)
$(SCRIPTS_DIR)/$(SCRIPTS_SERVER): $(SCRIPTS_SERVER)

$(SCRIPTS_DIR)/$(SCRIPTS_DGREP_DEST): scripts/$(SCRIPTS_DGREP)
$(SCRIPTS_DIR)/$(SCRIPTS_DGREP_SETUP_DEST): scripts/$(SCRIPTS_DGREP_SETUP)

$(SCRIPTS_DIR)/%:
	cp -f $< $@
	chmod +x $@

.PHONY: all clean client server

clean:
	rm client server obj/*
