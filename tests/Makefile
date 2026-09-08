# Makefile for trigger module host tests
#
# Build and run host-side tests for trigger.c UART handling logic

CC ?= gcc
CFLAGS = -Wall -Wextra -g -O2
LDFLAGS = 
SRC_DIR = .
OBJ_DIR = obj
BIN = test_trigger

SOURCES = test_main.c test_trigger.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean test

all: $(BIN)

$(BIN): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(BIN)

test: $(BIN)
	@echo "Running tests..."
	@./$(BIN)
