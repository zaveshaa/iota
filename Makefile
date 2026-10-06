CC        = cc
WARN      = -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wconversion
CFLAGS    = -std=c11 -D_POSIX_C_SOURCE=200809L -O2 $(WARN) -Iinclude
LDFLAGS   = -lm
PREFIX   ?= /usr/local

# CI builds with WERROR=1
ifeq ($(WERROR),1)
WARN     += -Werror
endif

BIN      ?= iota
CORE      = src/term.c src/render.c src/world.c src/cast.c src/view.c \
            src/engine.c src/scene.c
SRC       = $(CORE) demos/mirror.c
TEST_BIN  ?= golden
TEST_SRC   = $(CORE) tests/golden.c
HDR       = $(wildcard include/*.h)
GOLDEN    = tests/golden/mirror.txt

all: $(BIN)

$(BIN): $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

$(TEST_BIN): $(TEST_SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(TEST_SRC) $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN) $(GOLDEN)

regen: $(TEST_BIN)
	./$(TEST_BIN) --write $(GOLDEN)

run: $(BIN)
	./$(BIN)

install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin

help:
	@echo "iota — a ray tracer that renders inside the terminal"
	@echo ""
	@echo "  make            build $(BIN)"
	@echo "  make test       run the checks and compare the frame"
	@echo "  make regen      rewrite the golden frame"
	@echo "  make run        watch a mirror on the wall"
	@echo "  make install    install into $(DESTDIR)$(PREFIX)/bin"
	@echo "  make clean      remove the build"

clean:
	rm -f $(BIN) $(TEST_BIN)

.PHONY: all test regen run install help clean
