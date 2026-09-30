CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Iinclude
LDFLAGS ?=

BIN     ?= iota
SRC      = src/main.c src/term.c src/render.c src/camera.c

all: $(BIN)

$(BIN): $(SRC) $(wildcard include/*.h)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)

.PHONY: all run clean
