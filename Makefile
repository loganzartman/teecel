CC      = clang
CFLAGS  = -Wall -Wextra -g
TARGET  = teecel
SRC     = teecel.c
HEADERS = $(wildcard *.h)
STDIN   = test-stdin.txt

.PHONY: all build watch clean compile_commands

all: build compile_commands.json

build: $(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SRC)

compile_commands.json: Makefile
	@printf '[\n  {\n    "directory": "%s",\n    "file": "%s",\n    "arguments": ["%s", %s "-o", "%s", "%s"]\n  }\n]\n' \
		"$(CURDIR)" "$(SRC)" "$(CC)" \
		"$$(printf '"%s", ' $(CFLAGS))" \
		"$(TARGET)" "$(SRC)" > $@

watch:
	@which entr >/dev/null 2>&1 || { echo "error: 'entr' not found. install with: sudo apt install entr"; exit 1; }
	@test -f $(STDIN) || { echo "warning: $(STDIN) not found, creating empty file"; touch $(STDIN); }
	@echo "Watching $(SRC) $(HEADERS) $(STDIN) (Ctrl-C to stop)"
	@printf '%s\n' $(SRC) $(HEADERS) $(STDIN) | entr -c sh -c \
		'make build && echo "--- running ---" && ./$(TARGET) < $(STDIN)'

clean:
	rm -f $(TARGET) compile_commands.json
