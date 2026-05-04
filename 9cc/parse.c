#include "9cc.h"

Type *declspec(Token **rest, Token *token);
Type *declarator(Token **rest, Token *token, Type *ty);
Node *declaration(Token **rest, Token *token);
Node *compound_stmt(Token **rest, Token *token);
Node *expr_stmt(Token **rest, Token *token);
Node *expr(Token **rest, Token *token);
Node *assign(Token **rest, Token *token);
Node *equality(Token **rest, Token *token);
Node *relational(Token **rest, Token *token);
Node *add(Token **rest, Token *token);
Node *mul(Token **rest, Token *token);
Node *postfix(Token **rest, Token *token);
Node *unary(Token **rest, Token *token);
Node *primary(Token **rest, Token *token);

Obj *locals; // ローカル変数リストの先頭

bool equal(Token *token, char *op) {
  return memcmp(token->loc, op, token->len) == 0 && op[token->len] == '\0';
}

Token *skip(Token *token, char *op) {
  if (!equal(token, op)) {
    error_at(token->loc, "expected \"%s\"", op);
  }
  return token->next;
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進めて
// 真を返す。それ以外の場合には偽を返す。
bool consume(Token **rest, char *op, Token *token) {
  if (token->kind != TK_PUNCT || strlen(op) != token->len ||
      memcmp(token->loc, op, token->len)) {
    return false;
  }
  *rest = token->next;
  return true;
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進める。
// それ以外の場合にはエラーを報告する。
void expect(Token **rest, char *op, Token *token) {
  if (token->kind != TK_PUNCT || strlen(op) != token->len || 
      memcmp(token->loc, op, token->len)) {
    error_at(token->loc, "expected \"%s\"", op);
  }
  *rest = token->next;
}

// 次のトークンが数値の場合、トークンを1つ読み進めてその数値を返す。
// それ以外の場合にはエラーを報告する。
int expect_number(Token **rest, Token *token) {
  if (token->kind != TK_NUM) {
    error_at(token->loc, "expected a number");
  }
  int val = token->val;
  *rest = token->next;
  return val;
}

bool at_eof(Token *token) {
  return token->kind == TK_EOF;
}

Node *new_node(NodeKind kind, Token *token) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->tok = token;
  return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *token) {
  Node *node = new_node(kind, token);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *new_unary(NodeKind kind, Node *expr, Token *token) {
  Node *node = new_node(kind, token);
  node->lhs = expr;
  return node;
}

Node *new_num(int val, Token *token) {
  Node *node = new_node(ND_NUM, token);
  node->val = val;
  return node;
}

Node *new_var_node(Obj *var, Token *token) {
  Node *node = new_node(ND_VAR, token);
  node->var = var;
  return node;
}

char *get_ident(Token *token) {
  if (token->kind != TK_IDENT) {
    error_tok(token, "expected an identifier");
  }
  return strndup(token->loc, token->len);
}

int get_number(Token *token) {
  if (token->kind != TK_NUM)
    error_tok(token, "expected a number");
  return token->val;
}

Obj *new_lvar(char *name, Type *ty) {
  Obj *var = calloc(1, sizeof(Obj));
  var->name = name;
  var->ty = ty;
  var->offset = locals ? locals->offset + ty->size : ty->size;
  var->next = locals;
  locals = var;
  return var;
}

Token *consume_ident(Token **rest, Token *token) {
  if (token->kind != TK_IDENT) {
    return NULL;
  }
  Token *tok = token;
  *rest = token->next;
  return tok;
}

Obj *find_var(Token *token) {
  for (Obj *var = locals; var; var = var->next) {
    if (strlen(var->name) == token->len && !strncmp(token->loc, var->name, token->len)) {
      return var;
    }
  }
  return NULL;
}

// declspec = "int"
Type *declspec(Token **rest, Token *token) {
  *rest = skip(token, "int");
  return ty_int;
}

// func-params = (param ("," param)*)? ")"
// param       = declspec declarator
Type *func_params(Token **rest, Token *token, Type *ty) {
  Type head = {};
  Type *cur = &head;

  while (!equal(token, ")")) {
    if (cur != &head) {
      token = skip(token, ",");
    }
    // 基本の型を取得
    Type *base_ty = declspec(&token, token);
    // 最終的な型を取得(int *, int **などの場合も考慮)
    Type *param_ty = declarator(&token, token, base_ty);
    // param_tyはポインタの可能性もあるため、copy(malloc)して新しく生成する
    cur->next = copy_type(param_ty);
    cur = cur->next;
  }

  ty = func_type(ty);
  ty->params = head.next;
  *rest = token->next;
  return ty;
}


// type-suffix = "(" func-params
//             | "[" num "]" type-suffix
//             | ε
Type *type_suffix(Token **rest, Token *token, Type *ty) {
  if (equal(token, "(")) {
    // 引数の処理
    return func_params(rest, token->next, ty);
  }
  if (equal(token, "[")) {
    // 配列の処理
    int len = get_number(token->next);
    token = skip(token->next->next, "]");
    ty = type_suffix(rest, token, ty);
    return array_of(ty, len);
  }

  *rest = token;
  return ty;
}

// declarator = "*"* ident type-suffix
Type *declarator(Token **rest, Token *token, Type *ty) {
  while (consume(&token, "*", token)) {
    ty = pointer_to(ty);
  }

  if (token->kind != TK_IDENT) {
    error_tok(token, "expected a variable name");
  }
  ty = type_suffix(rest, token->next, ty);
  ty->name = token;
  return ty;
}

// declaration = declspec (declarator ("=" expr)? ("," declarator ("=" expr)?)*)? ";"
Node *declaration(Token **rest, Token *token) {
  Type *basety = declspec(&token, token);

  Node head = {};
  Node *cur = &head;
  int i = 0;

  while (!equal(token, ";")) {
    if (i++ > 0)
      token = skip(token, ",");

    Type *ty = declarator(&token, token, basety);
    Obj *var = new_lvar(get_ident(ty->name), ty);

    if (!equal(token, "="))
      continue;

    Node *lhs = new_var_node(var, ty->name);
    Node *rhs = assign(&token, token->next);
    Node *node = new_binary(ND_ASSIGN, lhs, rhs, token);
    cur = cur->next = new_unary(ND_EXPR_STMT, node, token);
  }

  Node *node = new_node(ND_BLOCK, token);
  node->body = head.next;
  *rest = token->next;
  return node;
}

Node *assign(Token **rest, Token *token) {
  // 識別子の直後に "=" が続く場合、未登録なら変数を作る
  if (token->kind == TK_IDENT && equal(token->next, "=")) {
    if (!find_var(token)) {
      new_lvar(strndup(token->loc, token->len), ty_int);
    }
  }

  Node *node = equality(&token, token);

  if (consume(&token, "=", token)) {
    node = new_binary(ND_ASSIGN, node, assign(&token, token), token);
  }

  *rest = token;
  return node;
}

Node *expr_stmt(Token **rest, Token *token) {
  if (equal(token, ";")) {
    // ";"だけのときは空文なので、次のトークンを返す
    *rest = token->next;
    return new_node(ND_BLOCK, token);
  }

  Node *node = new_node(ND_EXPR_STMT, token);
  node->lhs = expr(&token, token);
  *rest = skip(token, ";");
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
    if (consume(&token, "==", token)) {
      node = new_binary(ND_EQ, node, relational(&token, token), NULL);
    } else if (consume(&token, "!=", token)) {
      node = new_binary(ND_NE, node, relational(&token, token), NULL);
    } else {
      *rest = token;
      return node;
    }
  }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
Node *relational(Token **rest, Token *token) {
  Node *node = add(&token, token);

  for (;;) {
    if (consume(&token, "<", token)) {
      node = new_binary(ND_LT, node, add(&token, token), NULL);
    } else if (consume(&token, "<=", token)) {
      node = new_binary(ND_LE, node, add(&token, token), NULL);
    } else if (consume(&token, ">", token)) {
      node = new_binary(ND_LT, add(&token, token), node, NULL);
    } else if (consume(&token, ">=", token)) {
      node = new_binary(ND_LE, add(&token, token), node, NULL);
    } else {
      *rest = token;
      return node;
    }
  }
}

Node *new_add(Node *lhs, Node *rhs, Token *token) {
  add_type(lhs);
  add_type(rhs);

  // num + num
  if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
    return new_binary(ND_ADD, lhs, rhs, token);
  }

  if (lhs->ty->base && rhs->ty->base) {
    error_tok(token, "invalid operands");
  }

  // Canonicalize `num + ptr` to `ptr + num`.
  if (!lhs->ty->base && rhs->ty->base) {
    Node *tmp = lhs;
    lhs = rhs;
    rhs = tmp;
  }

  // ptr + num
  rhs = new_binary(ND_MUL, rhs, new_num(lhs->ty->base->size, token), token);
  return new_binary(ND_ADD, lhs, rhs, token);
}

Node *new_sub(Node *lhs, Node *rhs, Token *token) {
  add_type(lhs);
  add_type(rhs);

  // num - num
  if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
    return new_binary(ND_SUB, lhs, rhs, token);
  }

  // ptr - num
  if (lhs->ty->base && is_integer(rhs->ty)) {
    rhs = new_binary(ND_MUL, rhs, new_num(lhs->ty->base->size, token), token);
    add_type(rhs);
    Node *node = new_binary(ND_SUB, lhs, rhs, token);
    node->ty = lhs->ty;
    return node;
  }

  // ptr - ptr, which returns how many elements are between the two.
  if (lhs->ty->base && rhs->ty->base) {
    Node *node = new_binary(ND_SUB, lhs, rhs, token);
    node->ty = ty_int;
    return new_binary(ND_DIV, node, new_num(lhs->ty->base->size, token), token);
  }

  error_tok(token, "invalid operands");
}

// add = mul ("+" mul | "-" mul)*
Node *add(Token **rest, Token *token) {
  Node *node = mul(&token, token);

  for (;;) {
    Token *start = token;
    
    if (consume(&token, "+", token)) {
      node = new_add(node, mul(&token, token), start);
    } else if (consume(&token, "-", token)) {
      node = new_sub(node, mul(&token, token), start);
    } else {
      *rest = token;
      return node;
    }
  }
}

// mul = unary ("*" unary | "/" unary)*
Node *mul(Token **rest, Token *token) {
  Node *node = unary(&token, token);

  for (;;) {
    Token *start = token;

    if (consume(&token, "*", token)) {
      node = new_binary(ND_MUL, node, unary(&token, token), start);
    } else if (consume(&token, "/", token)) {
      node = new_binary(ND_DIV, node, unary(&token, token), start);
    } else {
      *rest = token;
      return node;
    }
  }
}

// unary = ("+" | "-" | "&" | "*")? unary | postfix
Node *unary(Token **rest, Token *token) {
  if (consume(&token, "+", token)) {
    return unary(rest, token);
  }
  if (consume(&token, "-", token)) {
    return new_unary(ND_NEG, unary(rest, token), token);
  }
  if (consume(&token, "&", token)) {
    return new_unary(ND_ADDR, unary(rest, token), token);
  }
  if (consume(&token, "*", token)) {
    return new_unary(ND_DEREF, unary(rest, token), token);
  }

  return postfix(rest, token);
}

// postfix = primary ("[" expr "]")*
Node *postfix(Token **rest, Token *token) {
  Node *node = primary(&token, token);

  while (equal(token, "[")) {
    // x[y] is short for *(x+y)
    Token *start = token;
    Node *idx = expr(&token, token->next);
    token = skip(token, "]");
    node = new_unary(ND_DEREF, new_add(node, idx, start), start);
  }
  *rest = token;
  return node;

}

// funcall = ident "(" (assign ("," assign)*)? ")"
Node *funcall(Token **rest, Token *token) {
  Token *start = consume_ident(&token, token);
  token = skip(token, "(");

  Node head = {};
  Node *cur = &head;

  while(!equal(token, ")")) {
    if (cur != &head) {
      expect(&token, ",", token);
    }
    cur->next = assign(&token, token);
    cur = cur->next;
  }

  expect(&token, ")", token);
  *rest = token;

  Node *node = new_node(ND_FUNCALL, start);
  node->funcname = strndup(start->loc, start->len);
  node->args = head.next;
  return node;
}

// primary = "(" expr ")"
//            | sizeof unary
//            | ident func-args?
//            | num
Node *primary(Token **rest, Token *token) {
  // "(" expr ")"
  if (consume(&token, "(", token)) {
    Node *node = expr(&token, token);
    expect(&token, ")", token);
    *rest = token;
    return node;
  }

  if (equal(token, "sizeof")) {
    Node *node = unary(rest, token->next);
    add_type(node);
    return new_num(node->ty->size, token);
  }

  Token *tok = consume_ident(&token, token);
  if (tok) {

    if (equal(token, "(")) {
      // 関数呼び出しの処理
      return funcall(rest, tok);
    }

    Obj *var = find_var(tok);
    if (!var) {
      error_tok(tok, "undefined variable");
    }
    *rest = token;
    return new_var_node(var, tok);
  }

  // そうでなければ数値のはず
  if (token->kind == TK_NUM) {
    Token *tok = token;
    return new_num(expect_number(rest, token), tok);
  }

  error_at(token->loc, "expected an expression");
}

// stmt = "return" expr ";"
//      | "if" "(" expr ")" stmt ("else" stmt)?
//      | "for" "(" expr-stmt expr? ";" expr? ")" stmt
//      | "while" "(" expr ")" stmt
Node *stmt(Token **rest, Token *token) {
  // return文の処理
  if (equal(token, "return")) {
    Node *node = new_node(ND_RETURN, token);
    node->lhs = expr(&token, token->next);
    expect(&token, ";", token);
    *rest = token;
    return node;
  }

  if (equal(token, "if")) {
    Node *node = new_node(ND_IF, token);
    token = skip(token, "if");
    expect(&token, "(", token);
    node->cond = expr(&token, token);
    expect(&token, ")", token);
    node->then = stmt(&token, token);
    if (equal(token, "else"))
      node->els = stmt(&token, token->next);
    *rest = token;
    return node;
  }

  if (equal(token, "for")) {
    token = skip(token, "for");
    expect(&token, "(", token);
    Node *node = new_node(ND_FOR, token);
    node->init = expr_stmt(&token, token);
    if (!equal(token, ";")) {
      node->cond = expr(&token, token);
    }
    expect(&token, ";", token);
    if (!equal(token, ")")) {
      node->inc = expr(&token, token);
    }
    expect(&token, ")", token);
    node->then = stmt(&token, token);
    *rest = token;
    return node;
  }

  if (equal(token, "while")) {
    // while文の処理
    token = skip(token, "while");
    expect(&token, "(", token);
    Node *node = new_node(ND_FOR, token); // for文と同じノードで表現する
    node->cond = expr(&token, token);
    expect(&token, ")", token);
    node->then = stmt(&token, token);
    *rest = token;
    return node;
  }

  // ブロックの処理
  if (equal(token, "{")) {
    return compound_stmt(rest, token->next);
  }

  // 式文の処理
  return expr_stmt(rest, token);
}

Node *compound_stmt(Token **rest, Token *token) {
  Node *node = new_node(ND_BLOCK, token);

  Node head = {};
  Node *cur = &head;

  while (!equal(token, "}")) {
    if (equal(token, "int"))
      cur = cur->next = declaration(&token, token);
    else
      cur = cur->next = stmt(&token, token);
    add_type(cur);
  }

  node->body = head.next;
  *rest = token->next; // consume "}"
  return node;
}

void create_param_lvars(Type *param) {
  if (param) {
    create_param_lvars(param->next);
    new_lvar(get_ident(param->name), param);
  }
}

Function *function(Token **rest, Token *token) {
  Type *ty = declspec(&token, token);
  ty = declarator(&token, token, ty);

  Function *fn = calloc(1, sizeof(Function));
  fn->name = get_ident(ty->name);
  create_param_lvars(ty->params);
  fn->params = locals;

  token = skip(token, "{");

  fn->body = compound_stmt(rest, token);
  return fn;
}

/*
stmt       = "return" expr ";"
              | "if" "(" expr ")" stmt ("else" stmt)?
              | "for" "(" expr-stmt expr? ";" expr? ")" stmt
              | "while" "(" expr ")" stmt
              | "{" compound-stmt"
              | expr-stmt
expr-stmt  = expr? ";"
expr       = assign
assign     = equality ("=" assign)?
equality   = relational ("==" relational | "!=" relational)*
relational = add ("<" add | "<=" add | ">" add | ">=" add)*
add        = mul ("+" mul | "-" mul)*
mul        = unary ("*" unary | "/" unary)*
unary      = ("+" | "-" | "*" | "&") unary
           | postfix
postfix    = primary ("[" expr "]")*
primary    = ident
              | "(" (assign ("," assign)*)? ")"
              | num
*/

Function *parse(Token *token) {
  Function head = {};
  Function *cur = &head;

  while (!at_eof(token)) {
    cur->next = function(&token, token);
    cur = cur->next;
  }

  return head.next;
}
