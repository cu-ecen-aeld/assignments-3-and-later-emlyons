# Default compiler
cc := gcc

# Cross-compiler target overrides cc
CROSS_COMPILE: cc := aarch64-none-linux-gnu-gcc

# Default target
all: writer

# Cross-compile target
CROSS_COMPILE: writer

writer: writer.o
	$(cc) writer.o -o writer

writer.o: clean
	$(cc) -c writer.c -o writer.o

clean:
	rm -f writer.o writer