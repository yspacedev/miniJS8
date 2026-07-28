# Compiler settings
CC = clang
CFLAGS = -Wall -Wextra -g -MMD -MP -I./miniJS8 -I.

# Target executable name
TARGET = miniJS8_test

# Build directory
BUILD_DIR = build

# Source directories (tells make where to look for .c files)
VPATH = miniJS8

# Source and Object files
SRCS = $(wildcard *.c) $(wildcard miniJS8/*.c)
OBJS = $(notdir $(SRCS:.c=.o))
OBJS_IN_BUILD = $(addprefix $(BUILD_DIR)/, $(OBJS))
DEPS = $(OBJS_IN_BUILD:.o=.d)

# Default target
all: $(BUILD_DIR) $(TARGET)

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link object files to create the executable
$(TARGET): $(OBJS_IN_BUILD)
	$(CC) $(CFLAGS) -o $@ $^

# Compile source files to object files in build folder
$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Include dependency files
-include $(DEPS)

# Declare non-file targets
.PHONY: all clean