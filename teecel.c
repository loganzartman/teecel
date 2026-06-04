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
  NODE_TYPE_OBJECT,
} TclNodeType;

typedef enum TclObjectType {
  OBJECT_TYPE_STRING,
  OBJECT_TYPE_NUMBER,
} TclObjectType;

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
      const TclObjectType type;
      
      union {
        struct {
          const char* value;
        } string;

        struct {
          const double value; 
        } number;
      } data;
    } object;
  } data;
} TclNode;

TclNode* create_node(const TclNode node) {
  TclNode* node_ptr = malloc(sizeof(TclNode));
  memcpy(node_ptr, &node, sizeof(TclNode));
  return node_ptr;
}

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
    
    args[index] = create_node((TclNode){
      .type = NODE_TYPE_OBJECT,
      .data.object.type = OBJECT_TYPE_STRING,
      .data.object.data.string.value = arg,
    });
  }

  return (TclParseResult) {
    .success = true,
    .result.node = create_node((TclNode) {
      .type = NODE_TYPE_COMMAND,
      .data.command.routine = routine,
      .data.command.n_args = n_args,
      .data.command.args = (const TclNode**) args,
    }),
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

  return (TclParseResult) {
    .success = true,
    .result.node = create_node((TclNode) {
      .type = NODE_TYPE_PROGRAM,
      .data.program.n_commands = n_commands,
      .data.program.commands = (const TclNode**)commands,
    }),
  };
}

TclParseResult parse(const char* src_) {
  LOG("parse");
  const char** src = &src_;
  return parse_program(src);
}

void print_node(const TclNode* node);

void print_node_program(const TclNode* program) {
  assert(program != NULL);
  assert(program->type == NODE_TYPE_PROGRAM);

  for (size_t i = 0; i < program->data.program.n_commands; ++i) {
    print_node(program->data.program.commands[i]);
  }
}

void print_node_command(const TclNode* command) {
  assert(command != NULL);
  assert(command->type == NODE_TYPE_COMMAND);
  
  printf("%s", command->data.command.routine);
  if (command->data.command.n_args > 0) {
    for (size_t i = 0; i < command->data.command.n_args; ++i) {
      printf(" ");
      print_node(command->data.command.args[i]);
    }
  }
  printf(";\n");
}

void print_node_object(const TclNode* object) {
  assert(object != NULL);
  assert(object->type == NODE_TYPE_OBJECT);

  switch (object->data.object.type) {
    case OBJECT_TYPE_STRING:
      printf("%s", object->data.object.data.string.value);
      break;
    case OBJECT_TYPE_NUMBER:
      printf("%f", object->data.object.data.number.value);
      break;
    default:
      LOG("print_node_object: invalid object type");
      break;
  }
}

void print_node(const TclNode* node) {
  assert(node != NULL);
  switch (node->type) {
    case NODE_TYPE_PROGRAM:
      print_node_program(node);
      break;
    case NODE_TYPE_COMMAND:
      print_node_command(node);
      break;
    case NODE_TYPE_OBJECT:
      print_node_object(node);
      break;
    default:
      LOG("print_node: invalid node type");
      break;
  }
}

typedef struct TclName {
  const char* name;
  const TclNode* value;
} TclName;

typedef struct TclEvalContext {
  TclName* names;
  size_t n_names;
} TclEvalContext;

const TclNode* eval_node(const TclNode* node, TclEvalContext* context);

const TclNode* eval_node_program(const TclNode* program, TclEvalContext* context) {
  assert(program != NULL);
  assert(context != NULL);
  assert(program->type == NODE_TYPE_PROGRAM);
  
  const TclNode* result = NULL;
  for (size_t i = 0; i < program->data.program.n_commands; ++i) {
    result = eval_node(program->data.program.commands[i], context);
  }
  return result;
}

const TclNode* eval_builtin_expr(const TclNode* command, TclEvalContext* context) {
  assert(command != NULL);
  assert(context != NULL);
  assert(command->type == NODE_TYPE_COMMAND);

  return NULL;
}

const TclNode* eval_builtin_set(const TclNode* command, TclEvalContext* context) {
  assert(command != NULL);
  assert(context != NULL);
  assert(command->type == NODE_TYPE_COMMAND);

  if (command->data.command.n_args != 2) {
    LOG("Invalid number of arguments for set: %zu", command->data.command.n_args);
    return create_node((TclNode) {
      .type = NODE_TYPE_OBJECT,
      .data.object.type = OBJECT_TYPE_STRING,
      .data.object.data.string.value = "",
    });
  }

  const char* name = command->data.command.args[0]->data.object.data.string.value;
  const TclNode* value = command->data.command.args[1];

  for (size_t i = 0; i < context->n_names; ++i) {
    if (strcmp(context->names[i].name, name) == 0) {
      context->names[i].value = value;
      return value;
    }
  }

  context->names = realloc(context->names, (context->n_names + 1) * sizeof(TclName));
  context->names[context->n_names] = (TclName) {
    .name = name,
    .value = value,
  };
  context->n_names++;
  return value;
}

const TclNode* eval_node_command(const TclNode* command, TclEvalContext* context) {
  assert(command != NULL);
  assert(context != NULL);
  assert(command->type == NODE_TYPE_COMMAND);
  
  for (size_t i = 0; i < context->n_names; ++i) {
    if (strcmp(context->names[i].name, command->data.command.routine) == 0) {
      LOG("Unsupported: call variable %s", command->data.command.routine);
      return create_node((TclNode) {
        .type = NODE_TYPE_OBJECT,
        .data.object.type = OBJECT_TYPE_STRING,
        .data.object.data.string.value = "",
      });
    }
  }

  if (strcmp(command->data.command.routine, "expr") == 0) {
    return eval_builtin_expr(command, context);
  }
  if (strcmp(command->data.command.routine, "set") == 0) {
    return eval_builtin_set(command, context);
  }

  LOG("Unsupported: command %s", command->data.command.routine);
  return create_node((TclNode) {
    .type = NODE_TYPE_OBJECT,
    .data.object.type = OBJECT_TYPE_STRING,
    .data.object.data.string.value = "",
  });
}

const TclNode* eval_node_object(const TclNode* object, TclEvalContext* context) {
  assert(object != NULL);
  assert(context != NULL);
  assert(object->type == NODE_TYPE_OBJECT);
  
  return object;
}

const TclNode* eval_node(const TclNode* node, TclEvalContext* context) {
  switch (node->type) {
    case NODE_TYPE_PROGRAM:
      return eval_node_program(node, context);
    case NODE_TYPE_COMMAND:
      return eval_node_command(node, context);
    case NODE_TYPE_OBJECT:
      return eval_node_object(node, context);
    default:
      LOG("eval_node_with_context: invalid node type");
      return NULL;
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

  TclParseResult parse_result = parse(input);
  if (!parse_result.success) {
    printf("%s\n", format_parse_error(parse_result.error.code));
    return -1;
  }

  LOG("parsed:");

  print_node(parse_result.result.node);

  TclEvalContext context = {
    .names = NULL,
    .n_names = 0,
  };
  const TclNode* eval_result = eval_node(parse_result.result.node, &context);

  LOG("evaluated:");

  print_node(eval_result);

  return 0;
}
