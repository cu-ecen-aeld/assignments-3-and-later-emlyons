# Default compiler (host compiler)
CC := gcc -g -O0

# Override CC if CROSS_COMPILE prefix is set
ifdef CROSS_COMPILE
CC := $(CROSS_COMPILE)gcc
endif

# Source files
SRC := aesdsocket.c queue.c thread_pool.c

# Object files
OBJ := $(SRC:.c=.o)

# Default target
all: aesdsocket

# Link the executable
aesdsocket: $(OBJ)
	$(CC) $(OBJ) -o $@

# Compile source files to object files
%.o: %.c
	$(CC) -c $< -o $@

# Clean target to remove build artifacts
clean:
	rm -f $(OBJ) aesdsocket
