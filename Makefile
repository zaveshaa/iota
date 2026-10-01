CC        = cc
CFLAGS    = -std=c11 -D_POSIX_C_SOURCE=200809L -O2 -Wall -Wextra -Iinclude
LDFLAGS   = -lm

BIN     ?= iota
SRC      = src/main.c src/term.c src/render.c src/camera.c src/world.c

all: $(BIN)

$(BIN): $(SRC) $(wildcard include/*.h)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)

.PHONY: all run clean


