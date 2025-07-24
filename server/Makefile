# Default compiler (host compiler)
CC := gcc -g -O0

# Override CC if CROSS_COMPILE prefix is set
ifdef CROSS_COMPILE
CC := $(CROSS_COMPILE)gcc
endif

# Default target
all: aesdsocket

# Link the executable
aesdsocket: aesdsocket.o
	$(CC) aesdsocket.o -o aesdsocket

# Compile the object file
aesdsocket.o: aesdsocket.c
	$(CC) -c aesdsocket.c -o aesdsocket.o

# Clean target to remove build artifacts
clean:
	rm -f aesdsocket.o aesdsocket
