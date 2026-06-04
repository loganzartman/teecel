#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <assert.h>

#define DEBUG
#ifdef DEBUG
#define LOG(x, ...) printf("[debug] "); \
  printf(x __VA_OPT__(,) __VA_ARGS__); \
  printf("\n");
#else
#define LOG(x, ...)
#endif

typedef enum TclNodeType {
  NODE_TYPE_PROGRAM,
  NODE_TYPE_COMMAND,
  NODE_TYPE_STRING,
} TclNodeType;

typedef struct TclNode {
  const enum TclNodeType type;

  union {
    struct {
      const size_t n_commands;
      const struct TclNode** commands;
    } program;

    struct {
      const char* routine;
      const size_t n_args;
      const struct TclNode** args;
    } command;

    struct {
      const char* value;
    } string;
  } data;
} TclNode;

typedef enum TclParseError {
  PARSE_ERROR_GENERIC,
  PARSE_ERROR_EXTRA_INPUT,
} TclParseError;

const char* format_parse_error(const TclParseError error) {
  switch (error) {
    case PARSE_ERROR_GENERIC:
      return "Generic parse error";
    case PARSE_ERROR_EXTRA_INPUT:
      return "Parse error: extra input";
    default:
      return "Unknown parse error";
  }
}

typedef struct TclParseResult {
  bool success;
  struct {
    TclNode* node;
  } result;
  struct {
    TclParseError code;
  } error;
} TclParseResult;

bool isws(const char c) {
  return c == ' ' || c == '\t';
}

bool isworddelim(const char c) {
  return isws(c) || c == '\n' || c == '\r' || c == ';';
}

/** 
 * Return the character and advance the pointer, as long as it's not NULL.
 * Prevents reading past the end of the string.
 */
char take_char(const char** src) {
  assert(src != NULL);
  assert(*src != NULL);

  char c = **src;
  if (c == 0) {
    return 0;
  }
  ++(*src);
  return c;
}

char peek_char(const char** src) {
  assert(src != NULL);
  assert(*src != NULL);

  return **src;
}

bool parse_ws(const char** src) {
  LOG("parse ws");
  bool found = false;
  while (isws(**src)) {
    found = true;
    take_char(src);
  }
  return found;
}

bool parse_delims(const char** src) {
  LOG("parse delims: %s", *src);
  bool found = false;
  while (isworddelim(peek_char(src))) {
    found = true;
    take_char(src);
  }
  return found;
}

const char* parse_word(const char** src) {
  LOG("parse word");
  const char* start = *src;
  const char* end = *src;

  while (!isworddelim(peek_char(&end))) {
    take_char(&end);
  }

  size_t length = end - start;
  if (length == 0) {
    return NULL;
  }

  *src = end;

  char* word = malloc(length + 1);
  strncpy(word, start, length);
  word[length] = '\0';
  LOG("parsed word: %s", word);
  return word;
}

TclParseResult parse_command(const char** src) {
  LOG("parse command");
  parse_ws(src);
  const char* const routine = parse_word(src);
  if (routine == NULL) {
    return (TclParseResult) {
      .success = false,
      .error.code = PARSE_ERROR_GENERIC,
    };
  }
  LOG("parsed routine: %s", routine);

  size_t n_args = 0;
  TclNode** args = NULL;
  const char* arg = NULL;
  while ((arg = (parse_ws(src) ? parse_word(src) : NULL)) != NULL) {
    LOG("parsed arg: %s", arg);
    size_t index = n_args++;
    args = realloc(args, n_args * sizeof(TclNode*));
    
    TclNode node = {
      .type = NODE_TYPE_STRING,
      .data.string.value = arg,
    };
    args[index] = malloc(sizeof(TclNode));
    memcpy(args[index], &node, sizeof(TclNode));
  }

  TclNode command = {
    .type = NODE_TYPE_COMMAND,
    .data.command.routine = routine,
    .data.command.n_args = n_args,
    .data.command.args = (const TclNode**) args,
  };
  TclNode* command_ptr = malloc(sizeof(TclNode));
  memcpy(command_ptr, &command, sizeof(TclNode));

  return (TclParseResult) {
    .success = true,
    .result.node = command_ptr,
  };
}

TclParseResult parse_program(const char** src) {
  LOG("parse program");

  size_t n_commands = 0;
  TclNode** commands = NULL;
  while (peek_char(src) != 0) {
    TclParseResult command = parse_command(src);
    if (!command.success) {
      break;
    }

    size_t index = n_commands++;
    commands = realloc(commands, n_commands * sizeof(TclNode*));
    commands[index] = command.result.node;

    if (!parse_delims(src)) {
      break;
    }
    LOG("src: %s", *src);
  }

  if (peek_char(src) != 0) {
    return (TclParseResult) {
      .success = false,
      .error.code = PARSE_ERROR_EXTRA_INPUT,
    };
  }

  TclNode program = {
    .type = NODE_TYPE_PROGRAM,
    .data.program.n_commands = n_commands,
    .data.program.commands = (const TclNode**)commands,
  };
  TclNode* program_ptr = malloc(sizeof(TclNode));
  memcpy(program_ptr, &program, sizeof(TclNode));

  return (TclParseResult) {
    .success = true,
    .result.node = program_ptr,
  };
}

TclParseResult parse(const char* src_) {
  LOG("parse");
  const char** src = &src_;
  return parse_program(src);
}

void print_node(const TclNode* node);

void print_node_program(const TclNode* program) {
  if (program->type != NODE_TYPE_PROGRAM) {
    LOG("invalid node type");
    return;
  }

  for (size_t i = 0; i < program->data.program.n_commands; ++i) {
    print_node(program->data.program.commands[i]);
  }
}

void print_node_command(const TclNode* command) {
  if (command->type != NODE_TYPE_COMMAND) {
    LOG("invalid node type");
    return;
  }
  
  printf("%s", command->data.command.routine);
  if (command->data.command.n_args > 0) {
    for (size_t i = 0; i < command->data.command.n_args; ++i) {
      printf(" ");
      print_node(command->data.command.args[i]);
    }
  }
  printf(";\n");
}

void print_node_string(const TclNode* string) {
  if (string->type != NODE_TYPE_STRING) {
    LOG("invalid node type");
    return;
  }

  printf("%s", string->data.string.value);
}

void print_node(const TclNode* node) {
  switch (node->type) {
    case NODE_TYPE_PROGRAM:
      print_node_program(node);
      break;
    case NODE_TYPE_COMMAND:
      print_node_command(node);
      break;
    case NODE_TYPE_STRING:
      print_node_string(node);
      break;
    default:
      LOG("print_node: invalid node type");
      break;
  }
}

const char* read_stdin() {
  size_t size = 64;
  char* buffer = malloc(size);

  size_t i = 0;
  int c;
  while ((c = getchar()) != EOF) {
    if (i >= size) {
      size *= 1.5;
      buffer = realloc(buffer, size);
    }
    buffer[i++] = c;
  }
  buffer[i] = 0;

  return buffer;
}

int main(int argc, char const *argv[]) {
  (void) argc;
  (void) argv;

  LOG("teecel 1.0");
  
  const char* input = read_stdin();

  TclParseResult result = parse(input);
  if (!result.success) {
    printf("%s\n", format_parse_error(result.error.code));
    return -1;
  }

  LOG("done parse");

  print_node(result.result.node);

  return 0;
}
