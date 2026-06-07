#pragma once
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
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
  bool found = false;
  while (isws(peek_char(src))) {
    found = true;
    take_char(src);
  }
  return found;
}

bool parse_delims(const char** src) {
  bool found = false;
  while (isworddelim(peek_char(src))) {
    found = true;
    take_char(src);
  }
  return found;
}

TclParseResult parse_word_literal(const char** src) {
  const char* start = *src;
  const char* end = *src;

  while (!isworddelim(peek_char(&end)) && peek_char(&end) != 0) {
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

  return (TclParseResult) {
    .success = true,
    .result.node = create_node((TclNode) {
      .type = NODE_TYPE_LITERAL,
      .data.literal.value = word,
    }),
  };
}

TclParseResult parse_word_variable(const char** src) {
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

void parse_arg_list(const char** src, size_t* n_args, TclNode*** args) {
  assert(n_args != NULL);
  assert(args != NULL);

  *n_args = 0;
  TclParseResult arg;
  while (true) {
    arg = parse_word(src);
    if (!arg.success) {
      break;
    }

    size_t index = (*n_args)++;
    (*args) = realloc((*args), (*n_args) * sizeof(TclNode*));
    (*args)[index] = arg.result.node;

    if (!parse_ws(src)) {
      break;
    }
  }
}

TclParseResult parse_command(const char** src) {
  parse_ws(src);
  TclParseResult name = parse_word(src);
  if (!name.success) {
    return name;
  }

  size_t n_args = 0;
  TclNode** args = NULL;
  if (parse_ws(src)) {
    parse_arg_list(src, &n_args, &args);
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
  size_t len;
  TclRepType rep_type;
  union {
    int64_t v_int;
    double v_double;
  } rep;
} TclVal;

TclVal create_val(const char* string) {
  return (TclVal) {
    .string = string,
    .len = strlen(string),
    .rep_type = REP_TYPE_NONE,
    .rep.v_int = 0,
  };
}

void print_val(TclVal val) {
  switch (val.rep_type) {
    case REP_TYPE_NONE:
      printf("%s", val.string);
      break;
    case REP_TYPE_INT:
      printf("%" PRId64, val.rep.v_int);
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

TclVal* context_lookup_name(const TclEvalContext* context, const char* name) {
  assert(context != NULL);
  assert(name != NULL);

  for (size_t i = 0; i < context->n_names; ++i) {
    if (strcmp(context->names[i].name, name) == 0) {
      return &context->names[i].value;
    }
  }

  return NULL;
}

TclVal eval_node(const TclNode* node, TclEvalContext* context);

const char* eval_token_as_string(const TclToken* token, TclEvalContext* context) {
  assert(token != NULL);

  switch (token->type) {
    case TOKEN_TYPE_STRING:
      return token->string.value;
    case TOKEN_TYPE_VAR: {
      TclVal* value = context_lookup_name(context, token->var.name);
      if (value == NULL) {
        LOG("undefined name %s", token->var.name);
        return "";
      }

      return value->string;
    }
    default:
      LOG("Unsupported token type %d", token->type);
      return "";
  }
}

const char* eval_template_as_string(const TclNode* node, TclEvalContext* context) {
  assert(node != NULL);
  assert(node->type == NODE_TYPE_TEMPLATE);

  size_t result_len = 0;
  char* result = NULL;

  for (size_t i = 0; i < node->data.template.n_tokens; ++i) {
    const char* str = eval_token_as_string(&node->data.template.tokens[i], context);
    result_len += strlen(str);
    result = realloc(result, result_len + 1);
    strcat(result, str);
  }
  
  return result;
}

const char* eval_node_as_string(const TclNode* node, TclEvalContext* context) {
  switch (node->type) {
    case NODE_TYPE_LITERAL:
      return node->data.literal.value;
    case NODE_TYPE_TEMPLATE:
      return eval_template_as_string(node, context);
    case NODE_TYPE_COMMAND:
    case NODE_TYPE_COMMAND_LIST:
      LOG("Unexpected node type: %d", node->type);
      return "";
  }
}

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

typedef enum ExprOperator {
  OP_NONE,
  OP_LPAREN,
  OP_RPAREN,
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
} ExprOperator;

ExprOperator expr_parse_op(const char* str) {
  if (!strcmp(str, "(")) {
    return OP_LPAREN;
  }
  if (!strcmp(str, ")")) {
    return OP_RPAREN;
  }
  if (!strcmp(str, "+")) {
    return OP_ADD;
  }
  if (!strcmp(str, "-")) {
    return OP_SUB;
  }
  if (!strcmp(str, "*")) {
    return OP_MUL;
  }
  if (!strcmp(str, "/")) {
    return OP_DIV;
  }
  return OP_NONE;
}

int expr_op_precedence(ExprOperator op) {
  switch (op) {
    case OP_NONE:
    case OP_LPAREN:
    case OP_RPAREN:
      return 0;
    case OP_ADD:
    case OP_SUB:
      return 1;
    case OP_MUL:
    case OP_DIV:
      return 2;
    default:
      LOG("unknown operator: %d", op);
      return 0;
  }
}

bool shimmer_double(TclVal* val) {
  if (val->rep_type == REP_TYPE_DOUBLE) {
    return true;
  }

  char* endptr = NULL;
  double result = strtod(val->string, &endptr);
  if (endptr != val->string + val->len) {
    return false;
  }

  val->rep_type = REP_TYPE_DOUBLE;
  val->rep.v_double = result;
  return true;
}

bool shimmer_int(TclVal* val) {
  if (val->rep_type == REP_TYPE_INT) {
    return true;
  }

  char* endptr = NULL;
  int64_t result = strtoll(val->string, &endptr, 10);
  if (endptr != val->string + val->len) {
    return false;
  }

  val->rep_type = REP_TYPE_INT;
  val->rep.v_int = result;
  return true;
}

bool shimmer_number_both(TclVal* a, TclVal* b) {
  bool a_is_int = shimmer_int(a);
  bool b_is_int = shimmer_int(b);

  if (a_is_int && b_is_int) {
    return true;
  } 

  bool a_is_double = shimmer_double(a);
  bool b_is_double = shimmer_double(b);

  return a_is_double && b_is_double;
}

#define apply_binop_number(OPERANDS, N_OPERANDS, OP) \
  { \
    assert(*(N_OPERANDS) >= 2); \
    TclVal* a = &(OPERANDS)[*(N_OPERANDS) - 2]; \
    TclVal* b = &(OPERANDS)[*(N_OPERANDS) - 1]; \
    bool is_number = shimmer_number_both(a, b); \
    if (!is_number) { \
      LOG("Addition of non-number values"); \
      return; \
    } \
    switch (a->rep_type) { \
      case REP_TYPE_DOUBLE: \
        a->rep.v_double = a->rep.v_double OP b->rep.v_double; \
        break; \
      case REP_TYPE_INT: \
        a->rep.v_int = a->rep.v_int OP b->rep.v_int; \
        break; \
      default: \
        LOG("Addition on unsupported types"); \
    } \
    --(*(N_OPERANDS)); \
  }

void expr_apply_op(TclVal* operands, size_t* n_operands, ExprOperator op) {
  assert(n_operands != NULL);

  switch (op) {
    case OP_ADD:
      apply_binop_number(operands, n_operands, +);
      break;
    case OP_SUB:
      apply_binop_number(operands, n_operands, -);
      break;
    case OP_MUL:
      apply_binop_number(operands, n_operands, *);
      break;
    case OP_DIV:
      apply_binop_number(operands, n_operands, /);
      break;
    case OP_NONE:
    case OP_LPAREN:
    case OP_RPAREN:
      LOG("applying invalid op %d", op);
      break;
  }
}

TclVal eval_builtin_expr(const TclNode* command, TclEvalContext* context) {
  assert(command != NULL);
  assert(context != NULL);
  assert(command->type == NODE_TYPE_COMMAND);

  // join args with space
  size_t src_len = 0;
  char* src = NULL;
  for (size_t i = 0; i < command->data.command.n_args; ++i) {
    const char* str = eval_node_as_string(command->data.command.args[i], context);
    src_len += strlen(str);
    if (i > 0) {
      src_len += 1;
    }
    src = realloc(src, src_len + 1);
    if (i == 0) {
      src[0] = '\0';
    } else {
      strcat(src, " ");
    }
    strcat(src, str);
  }

  // parse
  size_t n_args;
  TclNode** args;
  parse_arg_list((const char**)&src, &n_args, &args);
  if (n_args == 0) {
    return create_val("");
  }

  // shunting yard
  size_t n_operands = 0;
  TclVal* operands = NULL;
  size_t n_operators = 0;
  ExprOperator* operators = NULL;

  for (size_t i = 0; i < n_args; ++i) {
    TclVal val = create_val(eval_node_as_string(args[i], context));
    ExprOperator op = expr_parse_op(val.string);

    if (op == OP_NONE) {
      // push to operand stack
      operands = realloc(operands, ++n_operands * sizeof(*operands));
      operands[n_operands - 1] = val;
    } else if (op == OP_LPAREN) {
      // push to op stack
      operators = realloc(operators, ++n_operators * sizeof(*operators));
      operators[n_operators - 1] = op;
    } else if (op == OP_RPAREN) {
      while (true) {
        if (n_operators == 0) {
          LOG("expr: mismatched parenthesis");
          break;
        }

        ExprOperator top = operators[n_operators - 1];
        if (top == OP_LPAREN) {
          --n_operators;
          break;
        }

        expr_apply_op(operands, &n_operands, top);
        --n_operators;
      }
    } else {
      int precedence = expr_op_precedence(op);

      // apply higher-precedence ops
      while (true) {
        if (n_operators == 0) {
          break;
        }

        ExprOperator top = operators[n_operators - 1];
        if (top == OP_LPAREN) {
          break;
        }
        if (expr_op_precedence(top) < precedence) {
          break;
        }

        expr_apply_op(operands, &n_operands, top);
        --n_operators;
      }

      // push to op stack
      operators = realloc(operators, ++n_operators * sizeof(*operators));
      operators[n_operators - 1] = op;
    }
  }

  while (n_operators > 0) {
    ExprOperator top = operators[--n_operators];
    expr_apply_op(operands, &n_operands, top);
  }

  TclVal result = operands[0];

  free(operands);
  free(operators);
  return result;
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
