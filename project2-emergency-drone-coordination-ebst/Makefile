# Makefile for Emergency Drone Coordination System
UNAME := $(shell uname -s)

# Compiler and flags
CC = gcc
CFLAGS = -Wall -g -pthread -Iheaders
LDFLAGS = -ljson-c

# Platform-specific SDL2 flags
ifeq ($(UNAME), Linux)
	LDFLAGS += -lSDL2
endif
ifeq ($(UNAME), Darwin)
	LDFLAGS += -F/Library/Frameworks -framework SDL2
endif

# Source files
SERVER_SRC = server.c list.c map.c survivor.c ai.c view.c globals.c
CLIENT_SRC = drone_client.c
HEADERS = headers/list.h headers/map.h headers/drone.h headers/survivor.h headers/ai.h headers/coord.h headers/globals.h headers/view.h

# Object files
SERVER_OBJ = $(SERVER_SRC:.c=.o)
CLIENT_OBJ = $(CLIENT_SRC:.c=.o)

# Executables
SERVER = server
CLIENT = drone_client

# Default target
all: $(SERVER) $(CLIENT)

# Server executable
$(SERVER): $(SERVER_OBJ)
	$(CC) $(SERVER_OBJ) -o $(SERVER) $(LDFLAGS)

# Client executable
$(CLIENT): $(CLIENT_OBJ)
	$(CC) $(CLIENT_OBJ) -o $(CLIENT) $(LDFLAGS)

# Compile source files to object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f *.o $(SERVER) $(CLIENT)

# Phony targets
.PHONY: all clean