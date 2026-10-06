CC        = cc
WARN      = -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wconversion
CFLAGS    = -std=c11 -D_POSIX_C_SOURCE=200809L -O2 $(WARN) -Iinclude
LDFLAGS   = -lm
PREFIX   ?= /usr/local

# make WERROR=1 makes every warning a failure, which is how CI builds
ifeq ($(WERROR),1)
WARN     += -Werror
endif

BIN     ?= iota
SRC      = src/main.c src/term.c src/render.c src/camera.c src/world.c src/mesh.c src/gltf.c src/json.c

all: $(BIN)

$(BIN): $(SRC) $(wildcard include/*.h)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

run: $(BIN)
	./$(BIN)

install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin

help:
	@echo "iota — a 3D level editor that renders inside the terminal"
	@echo ""
	@echo "  make            build $(BIN)"
	@echo "  make run        run the editor"
	@echo "  make install    install into $(DESTDIR)$(PREFIX)/bin"
	@echo "  make clean      remove the build"

clean:
	rm -f $(BIN)

.PHONY: all run install help clean
