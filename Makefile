INC_DIR        := inc
CXX            := g++-9
CXXFLAGS       := -I$(INC_DIR) -std=c++17 -Wall -DDEBUG -O3 -g
LDFLAGS        := -pthread -rdynamic

OBJ_DIR        := obj
SRC_DIR        := src
MJE_SRC_DIR    := mje/src
TEST_DIR       := tests

SRC_FILES      := $(wildcard $(SRC_DIR)/*.cpp)
TEST_FILES     := $(wildcard $(TEST_DIR)/*.cpp)
SRC_OBJ_FILES  := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC_FILES))
TEST_OBJ_FILES := $(patsubst $(TEST_DIR)/%.cpp,$(OBJ_DIR)/%.test.o,$(TEST_FILES))
OBJ_FILES      := $(SRC_OBJ_FILES) $(TEST_OBJ_FILES)

MJE_SRC_FILES  := $(wildcard $(MJE_SRC_DIR)/*.cpp)
MJE_TARGETS    := mje/wc_maple mje/wc_juice

MAPLE          := maple
JUICE          := juice
MAPLEJUICE     := maplejuice
MJE            := mje

all: $(MAPLE) $(MAPLEJUICE) $(MJE) $(JUICE)

$(MAPLE): $(filter-out obj/maplejuice.o obj/juice.o, $(OBJ_FILES))
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(JUICE): $(filter-out obj/maplejuice.o obj/maple.o, $(OBJ_FILES))
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(MAPLEJUICE): $(filter-out obj/maple.o  obj/juice.o, $(OBJ_FILES))
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJ_DIR)/%.test.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(MJE)/%: $(MJE_SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(MJE): $(MJE_TARGETS)

.PHONY: all clean

clean:
	rm maple maplejuice juice $(MJE_TARGETS) obj/*
