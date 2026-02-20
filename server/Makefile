EXEC	?= aesdsocket
CC 		?= $(CROSS_COMPILE)gcc

# Flags
CFLAGS 	?= -g -Wall -Werror
LDFLAGS  ?= -pthread -lrt

SRC      = $(wildcard *.c)
OBJ      = $(SRC:.c=.o)
HEADERS  = $(wildcard *.h)

# Target
all: $(EXEC)

# Build
${EXEC}: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

# Compile
%.o: %.c %.h
	$(CC) -o $@ -c $< $(CFLAGS)

# Clean
.PHONY: clean
clean:
	@rm -rf $(EXEC) *.o

# Help
.PHONY: help
help:
	@@echo "\
Usage: make [target] [CROSS_COMPILE=<prefix>]\n\
\n\
Available targets: \n\
    all (default) - Build the aesdsocket application\n\
    clean         - Remove writer and all .o files\n\
    help          - Display this help message\n\
\n\
Cross-compilation:\n\
    make                                          # Native build\n\
    make CROSS_COMPILE=aarch64-none-linux-gnu-    # ARM64 cross-compile\n"
