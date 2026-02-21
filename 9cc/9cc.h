#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// tokenize.c
//

// トークンの種類
typedef enum {
  TK_IDENT,    // 変数
  TK_PUNCT,    // 区切り文字
  TK_NUM,      // 整数トークン
  TK_EOF,      // 入力の終わりを表すトークン
  TK_KEYWORD,  // キーワード
} TokenKind;

// トークン型
typedef struct Token Token;
struct Token {
  TokenKind kind; // トークンの型
  Token *next;    // 次の入力トークン
  int val;        // kindがTK_NUMの場合、その数値
  char *str;      // トークン文字列
  int len;        // トークンの長さ
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
Token *skip(Token *token, char *op);
Token *tokenize(char *input);

//
// parse.c
//

// 抽象構文木のノードの種類
typedef enum {
  ND_ADD,    // +
  ND_SUB,    // -
  ND_MUL,    // *
  ND_DIV,    // /
  ND_ASSIGN, // =
  ND_LVAR,   // ローカル変数
  ND_EQ,     // ==
  ND_NE,     // !=
  ND_LT,     // <
  ND_LE,     // <=
  ND_NUM,    // 整数
  ND_RETURN, // リターン文
  ND_BLOCK,  // { ... }
} NodeKind;


// 抽象構文木のノードの型
typedef struct Node Node;
struct Node {
  NodeKind kind; // ノードの型
  Node *next;   // 次のノード
  Node *lhs;     // 左辺
  Node *rhs;     // 右辺
  Node *body;    // ブロックの中身
  int val;       // kindがND_NUMの場合のみ使う
  int offset;    // kindがND_LVARの場合のみ使う
};

Node *parse(Token *tok);

//
// codegen.c
//
void codegen(Node *node);

typedef struct LVar LVar;

struct LVar {
  LVar *next;    // 次の変数かNULL
  char *name;    // 変数の名前
  int len;       // 変数の名前の長さ
  int offset;    // RBPからのオフセット
};
