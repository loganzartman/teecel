#include "teecel.h"

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
