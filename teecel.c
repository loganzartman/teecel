#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
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

typedef enum TclTokenType {
  TOKEN_TYPE_STRING,
  TOKEN_TYPE_VAR,
} TclTokenType;

typedef struct TclToken {
  const TclTokenType type;

  union {
    struct {
      const char* value;
    } string;

    struct {
      const char* name;
    } var;
  };
} TclToken;

TclToken* create_token(const TclToken token) {
  TclToken* token_ptr = malloc(sizeof(TclToken));
  memcpy(token_ptr, &token, sizeof(TclToken));
  return token_ptr;
}

typedef enum TclNodeType {
  NODE_TYPE_COMMAND_LIST,
  NODE_TYPE_COMMAND,
  NODE_TYPE_LITERAL,
  NODE_TYPE_TEMPLATE,
} TclNodeType;

typedef struct TclNode {
  const enum TclNodeType type;

  union {
    struct {
      const size_t n_commands;
      const struct TclNode** commands;
    } command_list;

    struct {
      const struct TclNode* name;
      const size_t n_args;
      const struct TclNode** args;
    } command;

    struct {
      const char* value;
    } literal;

    struct {
      const size_t n_tokens;
      const struct TclToken* tokens;
    } template;
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
  PARSE_ERROR_EXPECTED_WORD,
} TclParseError;

const char* format_parse_error(const TclParseError error) {
  switch (error) {
    case PARSE_ERROR_GENERIC:
      return "Generic parse error";
    case PARSE_ERROR_EXTRA_INPUT:
      return "Parse error: extra input";
    case PARSE_ERROR_EXPECTED_WORD:
      return "Parse error: expected word";
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

TclParseResult parse_word_literal(const char** src) {
  LOG("parse literal");
  const char* start = *src;
  const char* end = *src;

  while (!isworddelim(peek_char(&end))) {
    take_char(&end);
  }

  size_t length = end - start;
  if (length == 0) {
    return (TclParseResult) {
      .success = false,
      .error.code = PARSE_ERROR_EXPECTED_WORD,
    };
  }

  *src = end;

  char* word = malloc(length + 1);
  strncpy(word, start, length);
  word[length] = '\0';
  LOG("parsed word: %s", word);

  return (TclParseResult) {
    .success = true,
    .result.node = create_node((TclNode) {
      .type = NODE_TYPE_LITERAL,
      .data.literal.value = word,
    }),
  };
}

TclParseResult parse_word_variable(const char** src) {
  LOG("parse variable");
  if (peek_char(src) != '$') {
    return (TclParseResult) {
      .success = false,
      .error.code = PARSE_ERROR_GENERIC,
    };
  }
  take_char(src);

  TclParseResult literal = parse_word_literal(src);
  if (!literal.success) {
    return literal;
  }

  return (TclParseResult) {
    .success = true,
    .result.node = create_node((TclNode) {
      .type = NODE_TYPE_TEMPLATE,
      .data.template.n_tokens = 1,
      .data.template.tokens = create_token((TclToken) {
        .type = TOKEN_TYPE_VAR,
        .var.name = literal.result.node->data.literal.value,
      }),
    })
  };
}

TclParseResult parse_word(const char** src) {
  TclParseResult word = parse_word_variable(src);
  if (word.success) {
    return word;
  }

  word = parse_word_literal(src);
  if (word.success) {
    return word;
  }

  return (TclParseResult) {
    .success = false,
    .error.code = PARSE_ERROR_EXPECTED_WORD,
  };
}

TclParseResult parse_command(const char** src) {
  LOG("parse command");
  parse_ws(src);
  TclParseResult name = parse_word(src);
  if (!name.success) {
    return name;
  }

  size_t n_args = 0;
  TclNode** args = NULL;
  TclParseResult arg;
  while (true) {
    if (!parse_ws(src)) {
      break;
    }

    arg = parse_word(src);
    if (!arg.success) {
      break;
    }

    LOG("parsed arg");
    size_t index = n_args++;
    args = realloc(args, n_args * sizeof(TclNode*));
    args[index] = arg.result.node;
  }

  return (TclParseResult) {
    .success = true,
    .result.node = create_node((TclNode) {
      .type = NODE_TYPE_COMMAND,
      .data.command.name = name.result.node,
      .data.command.n_args = n_args,
      .data.command.args = (const TclNode**) args,
    }),
  };
}

TclParseResult parse_command_list(const char** src) {
  LOG("parse command list");

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
      .type = NODE_TYPE_COMMAND_LIST,
      .data.command_list.n_commands = n_commands,
      .data.command_list.commands = (const TclNode**)commands,
    }),
  };
}

TclParseResult parse(const char* src_) {
  LOG("parse");
  const char** src = &src_;
  return parse_command_list(src);
}

void print_node(const TclNode* node);

void print_node_command_list(const TclNode* command_list) {
  assert(command_list != NULL);
  assert(command_list->type == NODE_TYPE_COMMAND_LIST);

  for (size_t i = 0; i < command_list->data.command_list.n_commands; ++i) {
    print_node(command_list->data.command_list.commands[i]);
  }
}

void print_node_command(const TclNode* command) {
  assert(command != NULL);
  assert(command->type == NODE_TYPE_COMMAND);
  
  print_node(command->data.command.name);
  if (command->data.command.n_args > 0) {
    for (size_t i = 0; i < command->data.command.n_args; ++i) {
      printf(" ");
      print_node(command->data.command.args[i]);
    }
  }
  printf(";\n");
}

void print_node_literal(const TclNode* literal) {
  assert(literal != NULL);
  assert(literal->type == NODE_TYPE_LITERAL);

  printf("%s", literal->data.literal.value);
}

void print_token(const TclToken* token) {
  switch (token->type) {
    case TOKEN_TYPE_STRING:
      printf("%s", token->string.value);
      break;
    case TOKEN_TYPE_VAR:
      printf("$%s", token->var.name);
      break;
    default:
      LOG("print_token: invalid node type");
      break;
  }
}

void print_node_template(const TclNode* template) {
  assert(template != NULL);
  assert(template->type == NODE_TYPE_TEMPLATE);
  
  for (size_t i = 0; i < template->data.template.n_tokens; ++i) {
    print_token(&template->data.template.tokens[i]);
  }
}

void print_node(const TclNode* node) {
  switch (node->type) {
    case NODE_TYPE_COMMAND_LIST:
      print_node_command_list(node);
      break;
    case NODE_TYPE_COMMAND:
      print_node_command(node);
      break;
    case NODE_TYPE_LITERAL:
      print_node_literal(node);
      break;
    case NODE_TYPE_TEMPLATE:
      print_node_template(node);
      break; 
    default:
      LOG("print_node: invalid node type");
      break;
  }
}

typedef enum TclRepType {
  REP_TYPE_NONE,
  REP_TYPE_INT,
  REP_TYPE_DOUBLE,
} TclRepType;

typedef struct TclVal {
  const char* string;
  TclRepType rep_type;
  union {
    int64_t v_int;
    double v_double;
  } rep;
} TclVal;

TclVal create_val(const char* string) {
  return (TclVal) {
    .string = string,
    .rep_type = REP_TYPE_NONE,
  };
}

void print_val(TclVal val) {
  switch (val.rep_type) {
    case REP_TYPE_NONE:
      printf("%s", val.string);
      break;
    case REP_TYPE_INT:
      printf("%lld", val.rep.v_int);
      break;
    case REP_TYPE_DOUBLE:
      printf("%f", val.rep.v_double);
      break;
    default:
      LOG("print_val: invalid rep type");
      break;
  }
}

typedef struct TclName {
  const char* name;
  TclVal value;
} TclName;

typedef struct TclEvalContext {
  TclName* names;
  size_t n_names;
} TclEvalContext;

TclVal eval_node(const TclNode* node, TclEvalContext* context);

TclVal eval_node_command_list(const TclNode* command_list, TclEvalContext* context) {
  assert(command_list != NULL);
  assert(context != NULL);
  assert(command_list->type == NODE_TYPE_COMMAND_LIST);
  
  TclVal result = create_val("");
  for (size_t i = 0; i < command_list->data.command_list.n_commands; ++i) {
    result = eval_node(command_list->data.command_list.commands[i], context);
  }
  return result;
}

TclVal eval_builtin_expr(const TclNode* command, TclEvalContext* context) {
  assert(command != NULL);
  assert(context != NULL);
  assert(command->type == NODE_TYPE_COMMAND);

  return create_val("");
}

TclVal eval_builtin_set(const TclNode* command, TclEvalContext* context) {
  assert(command != NULL);
  assert(context != NULL);
  assert(command->type == NODE_TYPE_COMMAND);

  if (command->data.command.n_args != 2) {
    LOG("Invalid number of arguments for set: %zu", command->data.command.n_args);
    return create_val("");
  }

  const char* name = command->data.command.args[0]->data.literal.value;
  const TclNode* value_node = command->data.command.args[1];
  assert(value_node->type == NODE_TYPE_LITERAL);

  TclVal value = create_val(value_node->data.literal.value);

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

const char* eval_node_as_string(const TclNode* node, TclEvalContext* context) {
  switch (node->type) {
    case NODE_TYPE_LITERAL:
      return node->data.literal.value;
    case NODE_TYPE_TEMPLATE:
      LOG("not implemented");
      return "";
    case NODE_TYPE_COMMAND:
    case NODE_TYPE_COMMAND_LIST:
      LOG("Unexpected node type: %d", node->type);
      return "";
  }
}

TclVal eval_node_command(const TclNode* command, TclEvalContext* context) {
  assert(command != NULL);
  assert(context != NULL);
  assert(command->type == NODE_TYPE_COMMAND);

  const char* name = eval_node_as_string(command->data.command.name, context);
  if (strcmp(name, "expr") == 0) {
    return eval_builtin_expr(command, context);
  }
  if (strcmp(name, "set") == 0) {
    return eval_builtin_set(command, context);
  }

  LOG("Unsupported: command %s", name);
  return create_val("");
}

TclVal eval_node_literal(const TclNode* literal, TclEvalContext* context) {
  assert(literal != NULL);
  assert(context != NULL);
  assert(literal->type == NODE_TYPE_LITERAL);
  
  return create_val(literal->data.literal.value);
}

TclVal eval_node(const TclNode* node, TclEvalContext* context) {
  switch (node->type) {
    case NODE_TYPE_COMMAND_LIST:
      return eval_node_command_list(node, context);
    case NODE_TYPE_COMMAND:
      return eval_node_command(node, context);
    case NODE_TYPE_LITERAL:
      return eval_node_literal(node, context);
    default:
      LOG("eval_node_with_context: invalid node type");
      return create_val("");
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
  TclVal eval_result = eval_node(parse_result.result.node, &context);

  LOG("evaluated:");

  print_val(eval_result);

  return 0;
}
