#include "9cc.h"

Node *expr(Token **rest, Token *token);
Node *equality(Token **rest, Token *token);
Node *relational(Token **rest, Token *token);
Node *add(Token **rest, Token *token);
Node *mul(Token **rest, Token *token);
Node *unary(Token **rest, Token *token);
Node *primary(Token **rest, Token *token);



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
bool consume(char *op, Token *token) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len ||
      memcmp(token->str, op, token->len))
    return false;
  *token = *token->next;
  return true;
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進める。
// それ以外の場合にはエラーを報告する。
void expect(char *op, Token *token) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len || 
      memcmp(token->str, op, token->len))
    error_at(token->str, "expected \"%s\"", op);
  *token = *token->next;
}

// 次のトークンが数値の場合、トークンを1つ読み進めてその数値を返す。
// それ以外の場合にはエラーを報告する。
int expect_number(Token *token) {
  if (token->kind != TK_NUM)
    error_at(token->str, "expected a number");
  int val = token->val;
  *token = *token->next;
  return val;
}

bool at_eof(Token *token) {
  return token->kind == TK_EOF;
}


// expr = equality
Node *expr(Token **rest, Token *token) {
  return equality(rest, token);
}

// equality = relational ("==" relational | "!=" relational)*
Node *equality(Token **rest, Token *token) {
  Node *node = relational(rest, token);

  for (;;) {
    if (consume("==", token))
      node = new_node(ND_EQ, node, relational(rest, token));
    else if (consume("!=", token))
      node = new_node(ND_NE, node, relational(rest, token));
    else
      return node;
  }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
Node *relational(Token **rest, Token *token) {
  Node *node = add(rest, token);

  for (;;) {
    if (consume("<", token))
      node = new_node(ND_LT, node, add(rest, token));
    else if (consume("<=", token))
      node = new_node(ND_LE, node, add(rest, token));
    else if (consume(">", token))
      node = new_node(ND_LT, add(rest, token), node);
    else if (consume(">=", token))
      node = new_node(ND_LE, add(rest, token), node);
    else
      return node;
  }
}

// add = mul ("+" mul | "-" mul)*
Node *add(Token **rest, Token *token) {
  Node *node = mul(rest, token);

  for (;;) {
    if (consume("+", token))
      node = new_node(ND_ADD, node, mul(rest, token));
    else if (consume("-", token))
      node = new_node(ND_SUB, node, mul(rest, token));
    else
      return node;
  }
}

// mul = unary ("*" unary | "/" unary)*
Node *mul(Token **rest, Token *token) {
  Node *node = unary(rest, token);

  for (;;) {
    if (consume("*", token))
      node = new_node(ND_MUL, node, unary(rest, token));
    else if (consume("/", token))
      node = new_node(ND_DIV, node, unary(rest, token));
    else
      return node;
  }
}

// unary = ("+" | "-")? unary | primary
Node *unary(Token **rest, Token *token) {
  if (consume("+", token))
    return unary(rest, token);
  if (consume("-", token))
    return new_node(ND_SUB, new_node_num(0), unary(rest, token));
  return primary(rest, token);
}

// primary = "(" expr ")" | num
Node *primary(Token **rest, Token *token) {
  // 次のトークンが"("なら、"(" expr ")"のはず
  if (consume("(", token)) {
    Node *node = expr(rest, token);
    expect(")", token);
    return node;
  }

  // そうでなければ数値のはず
  return new_node_num(expect_number(token));
}

Node *parse(Token *token) {
  Node *node = expr(&token, token);
  if (token->kind != TK_EOF)
    error_at(token->str, "式の終わりではありません");
  return node;
}
