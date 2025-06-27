# Default compiler (host compiler)
CC := gcc

# Override CC if CROSS_COMPILE prefix is set
ifdef CROSS_COMPILE
CC := $(CROSS_COMPILE)gcc
endif

# Default target
all: writer

# Link the executable
writer: writer.o
	$(CC) writer.o -o writer

# Compile the object file
writer.o: writer.c
	$(CC) -c writer.c -o writer.o

# Clean target to remove build artifacts
clean:
	rm -f writer.o writer
