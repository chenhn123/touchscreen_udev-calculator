# Default target architecture
ARCH ?= native

# Source and object files
SRCS = calibrate.c
OBJS = $(SRCS:.c=.o)

# Executable name based on architecture
TARGET = calibrate

# Set compiler and flags based on target architecture
ifeq ($(ARCH), arm)
    CC = arm-linux-gnueabihf-gcc
    CFLAGS = -Wall -g -march=armv7-a
else ifeq ($(ARCH), arm64)
    CC = aarch64-linux-gnu-gcc
    CFLAGS = -Wall -g -march=armv8-a
else
    CC = gcc
    CFLAGS = -Wall -g
endif

# Default rule
all: $(TARGET)

# Linking
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compiling
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f *.o calibrate calibrate_arm calibrate_arm64

