#include "9cc.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    error("引数の個数が正しくありません");
    return 1;
  }

  // tokenize and parse
  Token *token = tokenize_file(argv[1]);
  Obj *prog = parse(token);

  codegen(prog);

  return 0;
}
