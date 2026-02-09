#include "9cc.h"

Node *expr(Token **rest, Token *token);
Node *equality(Token **rest, Token *token);
Node *relational(Token **rest, Token *token);
Node *add(Token **rest, Token *token);
Node *mul(Token **rest, Token *token);
Node *unary(Token **rest, Token *token);
Node *primary(Token **rest, Token *token);
LVar *find_lvar(Token *tok);

Node *code[100];
LVar *locals; // ローカル変数リストの先頭

Node *new_node_num(int val) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_NUM;
  node->val = val;
  return node;
}

Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進めて
// 真を返す。それ以外の場合には偽を返す。
bool consume(Token **rest, char *op, Token *token) {
  if (token->kind != TK_PUNCT || strlen(op) != token->len ||
      memcmp(token->str, op, token->len))
    return false;
  *rest = token->next;
  return true;
}


Token *consume_ident(Token **rest, Token *token) {
  if (token->kind != TK_IDENT)
    return NULL;
  Token *tok = token;
  *rest = token->next;
  return tok;
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進める。
// それ以外の場合にはエラーを報告する。
void expect(Token **rest, char *op, Token *token) {
  if (token->kind != TK_PUNCT || strlen(op) != token->len || 
      memcmp(token->str, op, token->len))
    error_at(token->str, "expected \"%s\"", op);
  *rest = token->next;
}

// 次のトークンが数値の場合、トークンを1つ読み進めてその数値を返す。
// それ以外の場合にはエラーを報告する。
int expect_number(Token **rest, Token *token) {
  if (token->kind != TK_NUM)
    error_at(token->str, "expected a number");
  int val = token->val;
  *rest = token->next;
  return val;
}

bool at_eof(Token *token) {
  return token->kind == TK_EOF;
}

LVar *find_lvar(Token *tok) {
  for (LVar *var = locals; var; var = var->next) {
    if (var->len == tok->len && !memcmp(var->name, tok->str, tok->len))
      return var;
  }
  return NULL;
}

Node *assign(Token **rest, Token *token) {
  Node *node = equality(&token, token);

  if (consume(&token, "=", token))
    node = new_node(ND_ASSIGN, node, assign(&token, token));

  *rest = token;
  return node;
}

// expr = assign
Node *expr(Token **rest, Token *token) {
  return assign(rest, token);
}

// equality = relational ("==" relational | "!=" relational)*
Node *equality(Token **rest, Token *token) {
  Node *node = relational(&token, token);

  for (;;) {
    if (consume(&token, "==", token))
      node = new_node(ND_EQ, node, relational(&token, token));
    else if (consume(&token, "!=", token))
      node = new_node(ND_NE, node, relational(&token, token));
    else {
      *rest = token;
      return node;
    }
  }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
Node *relational(Token **rest, Token *token) {
  Node *node = add(&token, token);

  for (;;) {
    if (consume(&token, "<", token))
      node = new_node(ND_LT, node, add(&token, token));
    else if (consume(&token, "<=", token))
      node = new_node(ND_LE, node, add(&token, token));
    else if (consume(&token, ">", token))
      node = new_node(ND_LT, add(&token, token), node);
    else if (consume(&token, ">=", token))
      node = new_node(ND_LE, add(&token, token), node);
    else {
      *rest = token;
      return node;
    }
  }
}

// add = mul ("+" mul | "-" mul)*
Node *add(Token **rest, Token *token) {
  Node *node = mul(&token, token);

  for (;;) {
    if (consume(&token, "+", token))
      node = new_node(ND_ADD, node, mul(&token, token));
    else if (consume(&token, "-", token))
      node = new_node(ND_SUB, node, mul(&token, token));
    else {
      *rest = token;
      return node;
    }
  }
}

// mul = unary ("*" unary | "/" unary)*
Node *mul(Token **rest, Token *token) {
  Node *node = unary(&token, token);

  for (;;) {
    if (consume(&token, "*", token))
      node = new_node(ND_MUL, node, unary(&token, token));
    else if (consume(&token, "/", token))
      node = new_node(ND_DIV, node, unary(&token, token));
    else {
      *rest = token;
      return node;
    }
  }
}

// unary = ("+" | "-")? unary | primary
Node *unary(Token **rest, Token *token) {
  if (consume(&token, "+", token))
    return unary(rest, token);
  if (consume(&token, "-", token))
    return new_node(ND_SUB, new_node_num(0), unary(rest, token));
  return primary(rest, token);
}

// primary = "(" expr ")" | num
Node *primary(Token **rest, Token *token) {
  Token *tok = consume_ident(&token, token);
  if (tok) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_LVAR;

    LVar *lvar = find_lvar(tok);
    if (lvar) {
      // すでに登録されている変数の場合
      node->offset = lvar->offset;
    } else {
      // 新しい変数の場合はLVarを作って連結リストに追加
      lvar = calloc(1, sizeof(LVar));
      lvar->next = locals;
      lvar->name = tok->str;
      lvar->len = tok->len;
      lvar->offset = (locals ? locals->offset : 0) + 8;
      node->offset = lvar->offset;
      locals = lvar;
    }

    *rest = token;
    return node;
  }

  // 次のトークンが"("なら、"(" expr ")"のはず
  if (consume(&token, "(", token)) {
    Node *node = expr(&token, token);
    expect(&token, ")", token);
    *rest = token;
    return node;
  }

  // そうでなければ数値のはず
  if (token->kind == TK_NUM) {
    *rest = token;
    return new_node_num(expect_number(rest, token));
  }

  error_at(token->str, "expected an expression");
}

Node *stmt(Token **rest, Token *token) {
  Node *node = expr(&token, token);
  expect(&token, ";", token);
  *rest = token;
  return node;
}

void program(Token **rest, Token *token) {
  int i = 0;
  while (!at_eof(token)) {
    code[i++] = stmt(&token, token);
  }
  code[i] = NULL;
  *rest = token;
}

Node **parse(Token *token) {
  for (int i = 0; i < 100; i++) {
    code[i] = NULL;
  }
  program(&token, token);
  return code;
}
