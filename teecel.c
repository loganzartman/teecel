#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

#define DEBUG
#ifdef DEBUG
#define LOG(x) printf("[debug] %s\n", (x));
#else
#define LOG(x)
#endif

typedef enum TclNodeType {
  NODE_TYPE_COMMAND,
  NODE_TYPE_STRING,
} TclNodeType;

typedef struct TclNode {
  const enum TclNodeType type;

  union {
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
} TclParseError;

const char* format_parse_error(const TclParseError error) {
  switch (error) {
    case PARSE_ERROR_GENERIC:
      return "Generic parse error";
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

bool isworddelim(const char c) {
  return isspace(c) || c == ';' || c == '\0';
}

bool parse_ws(const char** src, bool optional) {
  LOG("parse ws");
  bool success = optional;
  while (isspace(**src)) {
    success = true;
    ++(*src);
  }
  return success;
}

const char* parse_word(const char** src) {
  LOG("parse word");
  const char* start = *src;
  const char* end = *src;

  while (!isworddelim(*end)) {
    ++end;
  }

  size_t length = end - start;
  if (length == 0) {
    return NULL;
  }

  *src = end;

  char* word = malloc(length + 1);
  strncpy(word, start, length);
  word[length] = '\0';
  return word;
}

TclParseResult parse_command(const char** src) {
  LOG("parse command");
  const char* const routine = parse_word(src);
  if (routine == NULL) {
    return (TclParseResult) {
      .success = false,
      .error.code = PARSE_ERROR_GENERIC,
    };
  }

  size_t n_args = 0;
  TclNode** args = NULL;
  const char* arg = NULL;
  while ((arg = (parse_ws(src, false), parse_word(src))) != NULL) {
    LOG("parsed arg");
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

TclParseResult parse(const char* src_) {
  LOG("parse");
  const char** src = &src_;
  TclParseResult result;

  if ((result = parse_command(src)).success) {
    return result;
  }

  result.success = false;
  result.error.code = PARSE_ERROR_GENERIC;
  return result;
}

void print_tcl_node(const TclNode* node);

void print_tcl_node_command(const TclNode* command) {
  if (command->type != NODE_TYPE_COMMAND) {
    LOG("invalid node type");
    return;
  }
  printf("%s", command->data.command.routine);
  if (command->data.command.n_args > 0) {
    for (size_t i = 0; i < command->data.command.n_args; ++i) {
      printf(" ");
      print_tcl_node(command->data.command.args[i]);
    }
  }
  printf(";\n");
}

void print_tcl_node_string(const TclNode* string) {
  if (string->type != NODE_TYPE_STRING) {
    LOG("invalid node type");
    return;
  }
  printf("%s", string->data.string.value);
}

void print_tcl_node(const TclNode* node) {
  switch (node->type) {
    case NODE_TYPE_COMMAND:
      print_tcl_node_command(node);
      break;
    case NODE_TYPE_STRING:
      print_tcl_node_string(node);
      break;
    default:
      LOG("print_tcl_node: invalid node type");
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
    printf("Parse error: %s\n", format_parse_error(result.error.code));
    return -1;
  }

  LOG("done parse");

  print_tcl_node(result.result.node);

  return 0;
}
