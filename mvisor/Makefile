INCLUDE_DIRS := ./include /usr/include
LIBS := stdc++
LIBS += pthread SDL
MKDIR_P = mkdir -p

CCFLAGS = $(addprefix -I, $(INCLUDE_DIRS)) -Wall -Werror -fno-exceptions -O2 -g
BUILD_DIR = ../build

EXECUTABLE = $(BUILD_DIR)/mvisor
MV_SOURCE := $(wildcard *.cc)
MV_SOURCE += $(wildcard */*.cc)
MV_SOURCE += $(wildcard devices/*/*.cc)
MV_SOURCE += $(wildcard devices/block/*/*.cc)
MV_OBJECTS := $(MV_SOURCE:%.cc=$(BUILD_DIR)/%.o)

$(shell mkdir -p $(dir $(MV_OBJECTS)))

.PHONY: run all clean
run: all
	$(EXECUTABLE)

all: $(EXECUTABLE)

$(EXECUTABLE): $(MV_OBJECTS) $(BUILD_DIR)/keymap.o
	$(CC) -o $@ $^ $(addprefix -l, $(LIBS))

clean:
	$(RM) -rf $(BUILD_DIR)/*

$(MV_OBJECTS): $(BUILD_DIR)/%.o: %.cc
	$(CC) $(CCFLAGS) -c -o $@ $<

$(BUILD_DIR)/keymap.o: $(BUILD_DIR)/%.o: %.c
	$(CC) $(CCFLAGS) -c -o $@ $<

.cc.o:
	$(CC) $(CCFLAGS) -c -o $@ $<

