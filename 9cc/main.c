#include "9cc.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    error("引数の個数が正しくありません");
    return 1;
  }

  // トークナイズする
  Token *token = tokenize(argv[1]);
  Node **nodes = parse(token);

  // アセンブリの前半部分を出力
  printf(".intel_syntax noprefix\n");
  printf(".globl main\n");
  printf("main:\n");

  // プロローグ
  // 変数26個分の領域を確保する
  printf("  push rbp\n");
  printf("  mov rbp, rsp\n");
  printf("  sub rsp, 208\n");

  // 抽象構文木を下りながらコード生成
  // Node *nodeはNode *code[100]です。これを最初からループする処理を行う
  for (int i = 0; i < 100; i++) {
    if (nodes[i] == NULL) break;

    codegen(nodes[i]);

    // 最後の文以外は結果をスタックから取り除く
    // 最後の文の結果は戻り値として使用するためRAXに残す
    printf("  pop rax\n");
  }

  // エピローグ
  // 最後の式の結果がRAXに残っているのでそれが返り値になる
  printf("  mov rsp, rbp\n");
  printf("  pop rbp\n");
  printf("  ret\n");

  return 0;
}
