#include "9cc.h"

static int label_count = 0;
// 関数呼び出しの引数を格納するレジスタ
static char *argreg[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
Function *current_fn;

void push(void) {
  printf("  push rax\n");
  label_count++;
}

void pop(char *arg) {
  printf("  pop %s\n", arg);
  label_count--;
}

void codegen_lval(Node *node) {
  if (node->kind != ND_VAR)
    error("代入の左辺値が変数ではありません");

  // RBPからのオフセットに変数がある
  printf("  mov rax, rbp\n");
  printf("  sub rax, %d\n", node->var->offset);
}

void gen_expr(Node *node) {
  switch (node->kind) {
  case ND_NUM:
    printf("  mov rax, %d\n", node->val);
    return;
  case ND_NEG:
    // 単項マイナス: lhs を評価してから neg 命令で符号反転
    gen_expr(node->lhs);
    printf("  neg rax\n");
    return;
  case ND_VAR:
    // 左辺値としてpushした変数のアドレスをスタックにpush
    codegen_lval(node);
    printf("  mov rax, [rax]\n");
    return;
  case ND_ASSIGN:
    codegen_lval(node->lhs); // 左辺の変数のアドレスをraxにセット
    push();  // raxをスタックにpushして退避
    gen_expr(node->rhs); // 右辺の値をraxにセット
    pop("rdi"); // 左辺のアドレスをrdiにpop

    printf("  mov [rdi], rax\n"); // 右辺の値を左辺のアドレスに格納
    return;
  case ND_FUNCALL:
    int nargs = 0;
    for (Node *arg = node->args; arg; arg = arg->next) {
      gen_expr(arg);
      push();
      nargs++;
    }

    for(int i = nargs - 1; i >= 0; i--) {
      pop(argreg[i]);
    }
    printf("  mov rax, 0\n");
    printf("  call %s\n", node->funcname);
    return;
  }

  gen_expr(node->rhs);
  push();
  gen_expr(node->lhs);
  pop("rdi");

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
}

void gen_stmt(Node *node) {
  switch (node->kind){
  case ND_BLOCK:
    // ブロック内の各文を順番に処理
    for (Node *n = node->body; n; n = n->next) {
      gen_stmt(n);
    }
    return;

  case ND_NULL_STMT:
    // 空文は何もしない
    return;

  case ND_IF:
    int c = label_count++;
    // 条件式を評価してスタックにpush
    gen_expr(node->cond);
    printf("  cmp rax, 0\n");
    if (node->els) {
      printf("  je .L.else.%d\n", c);
      gen_stmt(node->then);
      printf("  jmp .L.end.%d\n", c);
      printf(".L.else.%d:\n", c);
      gen_stmt(node->els);
    } else {
      printf("  je .L.end.%d\n", c);
      gen_stmt(node->then);
    }
    printf(".L.end.%d:\n", c);
    return;

  case ND_FOR:
    c = label_count++;
    if (node->init)
      gen_stmt(node->init);
    printf(".L.begin.%d:\n", c);
    if (node->cond) {
      gen_expr(node->cond);
      printf("  cmp rax, 0\n");
      printf("  je .L.end.%d\n", c);
    }
    gen_stmt(node->then);
    if (node->inc)
      gen_expr(node->inc);
    printf("  jmp .L.begin.%d\n", c);
    printf(".L.end.%d:\n", c);
    return;

  case ND_RETURN:
    // return文の右辺(返す値)を計算
    // 結果はスタックのトップに push される
    gen_expr(node->lhs);
    // 関数末尾の共通エピローグへジャンプ
    // エピローグをインラインで重複生成せず、ラベルにjmpする
    printf("  jmp .L.return.%s\n", current_fn->name);
    return;
  case ND_EXPR_STMT:
    gen_expr(node->lhs);
    return;
  }

  error_tok(node->tok, "invalid statement");
}


void codegen(Function *prog) {
  printf(".intel_syntax noprefix\n");
  for (Function *fn = prog; fn; fn = fn->next) {
    // アセンブリの前半部分を出力
    printf("  .globl %s\n", fn->name);
    printf("%s:\n", fn->name);
    current_fn = fn;

    // プロローグ
    // 変数26個分の領域を確保する
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, 208\n");

    gen_stmt(fn->body);

    // エピローグ
    printf(".L.return.%s:\n", fn->name);
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
  }
}
