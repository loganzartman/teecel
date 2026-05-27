CC      = clang
CFLAGS  = -Wall -Wextra -g
TARGET  = teecel
SRC     = teecel.c
STDIN   = test-stdin.txt

.PHONY: all build watch clean compile_commands

all: build compile_commands.json

build: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

compile_commands.json: Makefile
	@printf '[\n  {\n    "directory": "%s",\n    "file": "%s",\n    "arguments": ["%s", %s "-o", "%s", "%s"]\n  }\n]\n' \
		"$(CURDIR)" "$(SRC)" "$(CC)" \
		"$$(printf '"%s", ' $(CFLAGS))" \
		"$(TARGET)" "$(SRC)" > $@

watch:
	@which entr >/dev/null 2>&1 || { echo "error: 'entr' not found. install with: sudo apt install entr"; exit 1; }
	@test -f $(STDIN) || { echo "warning: $(STDIN) not found, creating empty file"; touch $(STDIN); }
	@echo "Watching $(SRC) $(STDIN) (Ctrl-C to stop)"
	@printf '%s\n' $(SRC) $(STDIN) | entr -c sh -c \
		'$(CC) $(CFLAGS) -o $(TARGET) $(SRC) && echo "--- running ---" && ./$(TARGET) < $(STDIN)'

clean:
	rm -f $(TARGET) compile_commands.json
