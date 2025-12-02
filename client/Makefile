CC = gcc
CFLAGS = -Wall -Wextra -I. -g
LDFLAGS =

# Directories
BUILD_DIR = build
BIN_DIR = bin

# Module directories
PROTOCOL_DIR = protocol
UI_DIR = ui
HANDLE_DIR = handle

# Source files by module
PROTOCOL_SOURCES = $(PROTOCOL_DIR)/protocol.c
UI_SOURCES = $(UI_DIR)/ui.c
HANDLE_SOURCES = $(HANDLE_DIR)/handle.c
CLIENT_SOURCES = client.c
MAIN_SOURCES = main.c

# All sources
ALL_SOURCES = $(MAIN_SOURCES) \
              $(CLIENT_SOURCES) \
              $(PROTOCOL_SOURCES) \
              $(UI_SOURCES) \
              $(HANDLE_SOURCES)

# Object files
PROTOCOL_OBJECTS = $(BUILD_DIR)/protocol/protocol.o
UI_OBJECTS = $(BUILD_DIR)/ui/ui.o
HANDLE_OBJECTS = $(BUILD_DIR)/handle/handle.o
CLIENT_OBJECTS = $(BUILD_DIR)/client.o
MAIN_OBJECTS = $(BUILD_DIR)/main.o

ALL_OBJECTS = $(MAIN_OBJECTS) \
              $(CLIENT_OBJECTS) \
              $(PROTOCOL_OBJECTS) \
              $(UI_OBJECTS) \
              $(HANDLE_OBJECTS)

# Executable
TARGET = $(BIN_DIR)/exam_client

# Default target
.PHONY: all clean setup

all: setup $(TARGET)

# Create necessary directories
setup:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)
	@mkdir -p $(BUILD_DIR)/protocol $(BUILD_DIR)/ui $(BUILD_DIR)/handle

# Link the executable
$(TARGET): $(ALL_OBJECTS)
	$(CC) $(ALL_OBJECTS) -o $@ $(LDFLAGS)
	@echo "Built $(TARGET)"

# Compile main.c
$(BUILD_DIR)/main.o: main.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile client.c
$(BUILD_DIR)/client.o: client.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile protocol module
$(BUILD_DIR)/protocol/protocol.o: $(PROTOCOL_DIR)/protocol.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile ui module
$(BUILD_DIR)/ui/ui.o: $(UI_DIR)/ui.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile handle module
$(BUILD_DIR)/handle/handle.o: $(HANDLE_DIR)/handle.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "Cleaned build artifacts"

# Run the client
run: $(TARGET)
	./$(TARGET)
