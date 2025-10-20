#include "9cc.h"

void codegen_lval(Node *node) {
  if (node->kind != ND_LVAR)
    error("代入の左辺値が変数ではありません");

  // 変数のアドレスを計算してそれをスタックにpush
  // RBPからのオフセットに変数がある
  printf("  mov rax, rbp\n");
  printf("  sub rax, %d\n", node->offset);
  printf("  push rax\n");
}

void codegen(Node *node) {
  switch (node->kind){
  case ND_NUM:
    // 数値ならそのままスタックにpush
    printf("  push %d\n", node->val);
    return;
  case ND_LVAR:
    // 左辺値としてpushした変数のアドレスをスタックにpush
    codegen_lval(node);
    printf("  pop rax\n");
    printf("  mov rax, [rax]\n");
    printf("  push rax\n");
    return;
  case ND_ASSIGN:
    // 左辺値としてpushした変数のアドレスをスタックにpush
    codegen_lval(node->lhs);
    // 右辺値を計算してスタックにpush
    codegen(node->rhs);

    printf("  pop rdi\n"); // 右辺の値をRDIに
    printf("  pop rax\n"); // 左辺のアドレスをRAXに
    printf("  mov [rax], rdi\n"); // 右辺の値を左辺のアドレスに格納
    printf("  push rdi\n"); // 式全体の値として右辺の値をpush
    return;
  }

  codegen(node->lhs);
  codegen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  switch (node->kind) {
  case ND_ADD:
    printf("  add rax, rdi\n");
    break;
  case ND_SUB:
    printf("  sub rax, rdi\n");
    break;
  case ND_MUL:
    printf("  imul rax, rdi\n");
    break;
  case ND_DIV:
    printf("  cqo\n");
    printf("  idiv rdi\n");
    break;
  case ND_EQ:
    printf("  cmp rax, rdi\n");
    printf("  sete al\n");
    printf("  movzb rax, al\n");
    break;
  case ND_NE:
    printf("  cmp rax, rdi\n");
    printf("  setne al\n");
    printf("  movzb rax, al\n");
    break;
  case ND_LT:
    printf("  cmp rax, rdi\n");
    printf("  setl al\n");
    printf("  movzb rax, al\n");
    break;
  case ND_LE:
    printf("  cmp rax, rdi\n");
    printf("  setle al\n");
    printf("  movzb rax, al\n");
    break;
  }

  printf("  push rax\n");
}
