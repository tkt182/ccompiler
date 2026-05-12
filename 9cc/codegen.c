#include "9cc.h"

static int label_count = 0;
// 関数呼び出しの引数を格納するレジスタ
static char *argreg8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static char *argreg64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
Obj *current_fn;

void gen_expr(Node *node);
void gen_stmt(Node *node);

void push(void) {
  printf("  push rax\n");
  label_count++;
}

void pop(char *arg) {
  printf("  pop %s\n", arg);
  label_count--;
}

int align_to(int n, int align) {
  return (n + align - 1) / align * align;
}

void gen_addr(Node *node) {
  switch (node->kind) {
  case ND_VAR:
    if (node->var->is_local) {
      printf("  lea rax, [rbp - %d]\n", node->var->offset);
    } else {
      printf("  lea rax, [rip + %s]\n", node->var->name);
    }
    return;
  case ND_DEREF:
    gen_expr(node->lhs);
    return;
  }

  error_tok(node->tok, "not an lvalue");
}

// raxが指すメモリアドレスから値を読み取り、raxレジスタに格納する
void load(Type *ty) {
  if (ty->kind == TY_ARRAY) {
    return;
  }
  // メモリ -> rax(読み込み)
  if (ty->size == 1) {
    printf("  movzx rax, byte ptr [rax]\n");
  } else {
    printf("  mov rax, [rax]\n");
  }
}

void store(Type *ty) {
  pop("rdi");

  if (ty->size == 1) {
    printf("  mov [rdi], al\n");
  } else {
    printf("  mov [rdi], rax\n");
  }
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
    gen_addr(node);
    load(node->ty);
    return;
  case ND_DEREF:
    gen_expr(node->lhs);
    load(node->ty);
    return;
  case ND_ADDR:
    gen_addr(node->lhs);
    return;
  case ND_ASSIGN:
    gen_addr(node->lhs); // 左辺の変数のアドレスをraxにセット
    push();
    gen_expr(node->rhs); // 右辺の値をraxにセット
    store(node->ty);
    return;
  case ND_STMT_EXPR:
    for (Node *n = node->body; n; n = n->next) {
      gen_stmt(n);
    }
    return;
  case ND_FUNCALL:
    int nargs = 0;
    // 全引数をスタックに積む
    for (Node *arg = node->args; arg; arg = arg->next) {
      gen_expr(arg);
      push();
      nargs++;
    }
    // 逆順にスタックからレジスタに値を移す
    for(int i = nargs - 1; i >= 0; i--) {
      pop(argreg64[i]);
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

void assign_lvar_offsets(Obj *prog) {
  for (Obj *fn = prog; fn; fn = fn->next) {
    if (!fn->is_function) {
      continue;
    }

    int offset = 0;
    for (Obj *var = fn->locals; var; var = var->next) {
      offset += var->ty->size;
      var->offset = offset;
    }
    fn->stack_size = align_to(offset, 16);
  }
}

void emit_data(Obj *prog) {
  for (Obj *var = prog; var; var = var->next) {
    if (var->is_function) {
      continue;
    }

    printf("  .data\n");
    printf("  .globl %s\n", var->name);
    printf("%s:\n", var->name);
    if (var->init_data) {
      for (int i = 0; i < var->ty->size; i++) {
        printf("  .byte %d\n", var->init_data[i]);
      }
    } else {
      printf("  .zero %d\n", var->ty->size);
    }
  }
}

void emit_text(Obj *prog) {
  for (Obj *fn = prog; fn; fn = fn->next) {
    if (!fn->is_function) {
      continue;
    }

    printf("  .globl %s\n", fn->name);
    printf("  .text\n");
    printf("%s:\n", fn->name);
    current_fn = fn;

    // プロローグ
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", fn->stack_size);

    // 引数をスタックにpush
    int i = 0;
    for (Obj *var = fn->params; var; var = var->next) {
      if (var->ty->size == 1) {
        printf("  mov [rbp - %d], %s\n", var->offset, argreg8[i++]);
      } else {
        printf("  mov [rbp - %d], %s\n", var->offset, argreg64[i++]);
      }
    }

    gen_stmt(fn->body);

    // エピローグ
    printf(".L.return.%s:\n", fn->name);
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
  }
}

void codegen(Obj *prog) {
  printf(".intel_syntax noprefix\n");

  assign_lvar_offsets(prog);
  emit_data(prog);
  emit_text(prog);
}
