# Makefile for Emergency Drone Coordination System (macOS version)

UNAME := $(shell uname -s)

# Get Homebrew prefix for json-c and SDL2
JSON_C_PREFIX := $(shell brew --prefix json-c)
SDL2_PREFIX := $(shell brew --prefix sdl2)

# Compiler and flags
CC = gcc
CFLAGS = -Wall -g -pthread
INCLUDES = -I. -Iheaders -I$(JSON_C_PREFIX)/include -I$(SDL2_PREFIX)/include
LDFLAGS = -L$(JSON_C_PREFIX)/lib -L$(SDL2_PREFIX)/lib
LIBS = -ljson-c -lSDL2

# Source files
COMMON_SRCS = list.c map.c survivor.c ai.c globals.c communication.c drone.c view.c
SERVER_SRCS = server.c $(COMMON_SRCS)
CLIENT_SRCS = drone_client.c communication.c list.c
HEADERS = headers/list.h headers/map.h headers/drone.h headers/survivor.h headers/ai.h headers/coord.h headers/globals.h headers/view.h headers/communication.h

# Object files
SERVER_OBJS = $(SERVER_SRCS:.c=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

# Executables
all: server client

server: $(SERVER_OBJS)
	$(CC) $(SERVER_OBJS) -o $@ $(LDFLAGS) $(LIBS)

client: $(CLIENT_OBJS)
	$(CC) $(CLIENT_OBJS) -o $@ $(LDFLAGS) $(LIBS)

# Compile source files to object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean up
clean:
	rm -f *.o server client

# Phony targets
.PHONY: all clean
