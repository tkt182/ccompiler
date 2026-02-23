#include "9cc.h"

Node *compound_stmt(Token **rest, Token *token);
Node *expr_stmt(Token **rest, Token *token);
Node *expr(Token **rest, Token *token);
Node *equality(Token **rest, Token *token);
Node *relational(Token **rest, Token *token);
Node *add(Token **rest, Token *token);
Node *mul(Token **rest, Token *token);
Node *unary(Token **rest, Token *token);
Node *primary(Token **rest, Token *token);
LVar *find_lvar(Token *tok);

LVar *locals; // ローカル変数リストの先頭


bool equal(Token *tok, char *op) {
  return memcmp(tok->str, op, tok->len) == 0 && op[tok->len] == '\0';
}

Token *skip(Token *token, char *op) {
  if (!equal(token, op))
    error_at(token->str, "expected \"%s\"", op);
  return token->next;
}

Node *new_node_num(int val) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_NUM;
  node->val = val;
  return node;
}

Node *new_node(NodeKind kind, Node *lhs, Node *rhs, Node *body) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->lhs = lhs;
  node->rhs = rhs;
  node->body = body;
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
    node = new_node(ND_ASSIGN, node, assign(&token, token), NULL);

  *rest = token;
  return node;
}

Node *expr_stmt(Token **rest, Token *token) {
  Node *node = expr(&token, token);
  expect(&token, ";", token);
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
      node = new_node(ND_EQ, node, relational(&token, token), NULL);
    else if (consume(&token, "!=", token))
      node = new_node(ND_NE, node, relational(&token, token), NULL);
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
      node = new_node(ND_LT, node, add(&token, token), NULL);
    else if (consume(&token, "<=", token))
      node = new_node(ND_LE, node, add(&token, token), NULL);
    else if (consume(&token, ">", token))
      node = new_node(ND_LT, add(&token, token), node, NULL);
    else if (consume(&token, ">=", token))
      node = new_node(ND_LE, add(&token, token), node, NULL);
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
      node = new_node(ND_ADD, node, mul(&token, token), NULL);
    else if (consume(&token, "-", token))
      node = new_node(ND_SUB, node, mul(&token, token), NULL);
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
      node = new_node(ND_MUL, node, unary(&token, token), NULL);
    else if (consume(&token, "/", token))
      node = new_node(ND_DIV, node, unary(&token, token), NULL);
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
    return new_node(ND_SUB, new_node_num(0), unary(rest, token), NULL);
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

// stmt = "return" expr ";" | "{" compound-stmt" | expr-stmt
Node *stmt(Token **rest, Token *token) {
  // return文の処理
  if (equal(token, "return")) {
    token = skip(token, "return");
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_RETURN;
    node->lhs = expr(&token, token);
    expect(&token, ";", token);
    *rest = token;
    return node;
  }

  // ブロックの処理
  if (equal(token, "{")) {
    return compound_stmt(rest, token);
  }

  // 式文の処理
  return expr_stmt(rest, token);
}

Node *compound_stmt(Token **rest, Token *token) {
  Node head = {};
  Node *cur = &head;

  expect(&token, "{", token);
  while (!consume(&token, "}", token)) {
    cur->next = stmt(&token, token);
    cur = cur->next;
  }

  Node *node = new_node(ND_BLOCK, NULL, NULL, head.next);

  *rest = token;
  return node;
}

/*
program    = stmt*
stmt       = "return" expr ";" | "{" compound-stmt" | expr-stmt
expr-stmt  = expr ";"
expr       = assign
assign     = equality ("=" assign)?
equality   = relational ("==" relational | "!=" relational)*
relational = add ("<" add | "<=" add | ">" add | ">=" add)*
add        = mul ("+" mul | "-" mul)*
mul        = unary ("*" unary | "/" unary)*
unary      = ("+" | "-")? primary
primary    = num | ident | "(" expr ")"
*/ 

Node *program(Token **rest, Token *token) {
  Node head = {};
  Node *cur = &head;

  while (!at_eof(token)) {
    cur->next = stmt(&token, token);
    cur = cur->next;
  }

  *rest = token;
  return head.next;
}

Node *parse(Token *token) {
  return program(&token, token);
}
