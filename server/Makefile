CC = gcc
CFLAGS = -Wall -Wextra -pthread -I/usr/include/mysql -g
LDFLAGS = -lmysqlclient -lssl -lcrypto -lpthread

# MySQL config
MYSQL_CFLAGS = $(shell mysql_config --cflags)
MYSQL_LIBS = $(shell mysql_config --libs)

# Directories
SRC_DIR = .
BUILD_DIR = build
BIN_DIR = bin

# Source files
SOURCES = main.c \
          server.c \
          protocol.c \
          database.c \
          auth.c \
          room.c \
          exam.c \
          practice.c \
          logger.c

# Object files
OBJECTS = $(SOURCES:%.c=$(BUILD_DIR)/%.o)

# Executable
TARGET = $(BIN_DIR)/exam_server

# Default target
.PHONY: all clean setup

all: setup $(TARGET)

# Create necessary directories
setup:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

# Link the executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) $(MYSQL_LIBS)
	@echo "Built $(TARGET)"

# Compile source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/*/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(MYSQL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(MYSQL_CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "Cleaned build artifacts"

# Run the server
run: $(TARGET)
	./$(TARGET)
