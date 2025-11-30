CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread
LDFLAGS = -lmysqlclient -lpthread

# Directories
SRC_DIR = .
DB_DIR = database
AUTH_DIR = auth
EXAM_DIR = exam
LOGGER_DIR = logger
PRACTICE_DIR = practice
PROTOCOL_DIR = protocol
ROOM_DIR = room

# Source files
SOURCES = main.c \
          $(DB_DIR)/database.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Target executable
TARGET = test_db

# Default target
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compile .c files to .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJECTS) $(TARGET)

# Install MySQL development libraries (for Ubuntu/WSL)
install-deps:
	sudo apt-get update
	sudo apt-get install -y libmysqlclient-dev build-essential

# Run the test
test: $(TARGET)
	./$(TARGET)

# Help
help:
	@echo "Available targets:"
	@echo "  all         - Build the test program"
	@echo "  clean       - Remove build files"
	@echo "  install-deps- Install required dependencies"
	@echo "  test        - Build and run the test program"
	@echo "  help        - Show this help message"

.PHONY: all clean install-deps test help
