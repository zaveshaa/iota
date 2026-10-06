CC        = cc
WARN      = -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wconversion
CFLAGS    = -std=c11 -D_POSIX_C_SOURCE=200809L -O2 $(WARN) -Iinclude
LDFLAGS   = -lm
PREFIX   ?= /usr/local

# make WERROR=1 makes every warning a failure, which is how CI builds
ifeq ($(WERROR),1)
WARN     += -Werror
endif

BIN      ?= iota
CORE      = src/term.c src/render.c src/world.c src/cast.c src/view.c \
            src/engine.c src/scene.c
SRC       = $(CORE) demos/mirror.c
HDR       = $(wildcard include/*.h)

all: $(BIN)

$(BIN): $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

run: $(BIN)
	./$(BIN)

install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin

help:
	@echo "iota — a ray tracer that renders inside the terminal"
	@echo ""
	@echo "  make            build $(BIN)"
	@echo "  make run        watch a mirror on the wall"
	@echo "  make install    install into $(DESTDIR)$(PREFIX)/bin"
	@echo "  make clean      remove the build"

clean:
	rm -f $(BIN)

.PHONY: all run install help clean
