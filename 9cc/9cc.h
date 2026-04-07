#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Type Type;

//
// tokenize.c
//

// トークンの種類
typedef enum {
  TK_IDENT,    // 変数
  TK_PUNCT,    // 区切り文字
  TK_KEYWORD,  // キーワード
  TK_NUM,      // 整数トークン
  TK_EOF,      // 入力の終わりを表すトークン
} TokenKind;

// トークン型
typedef struct Token Token;
struct Token {
  TokenKind kind; // トークンの型
  Token *next;    // 次の入力トークン
  int val;        // kindがTK_NUMの場合、その数値
  char *loc;      // トークン文字列の位置
  int len;        // トークンの長さ
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);
Token *skip(Token *token, char *op);
Token *tokenize(char *input);

//
// parse.c
//

// Local variable
typedef struct Obj Obj;
struct Obj {
  Obj *next;
  char *name; // 変数名
  Type *ty;   // 型
  int offset; // RBPからのオフセット
};


// 抽象構文木のノードの種類
typedef enum {
  ND_ADD,       // +
  ND_SUB,       // -
  ND_MUL,       // *
  ND_DIV,       // /
  ND_NEG,       // 単項 -
  ND_ASSIGN,    // =
  ND_VAR,       // ローカル変数
  ND_EQ,        // ==
  ND_NE,        // !=
  ND_LT,        // <
  ND_LE,        // <=
  ND_NUM,       // 整数
  ND_RETURN,    // リターン文
  ND_IF,        // if文
  ND_FOR,       // for文 or while文
  ND_BLOCK,     // { ... }
  ND_FUNCALL,   // 関数呼び出し
  ND_EXPR_STMT,  // 式文
  ND_NULL_STMT, // 空文
} NodeKind;

// 抽象構文木のノードの型
typedef struct Node Node;
struct Node {
  NodeKind kind;  // ノードの型
  Node *next;     // 次のノード
  Type *ty;       // ノードタイプ(今はint型のみ)
  Token *tok;     // 代表トークン

  Node *lhs;      // 左辺
  Node *rhs;      // 右辺
  Node *cond;     // if文の条件式
  Node *then;     // if文の真のときの式
  Node *els;      // if文の偽のときの式
  Node *init;     // for文の初期化式
  Node *inc;      // for文の増分
  Node *body;     // ブロックの中身
  char *funcname; // 関数呼び出しの関数名
  Node *args;     // 関数呼び出しの引数
  int val;        // kindがND_NUMの場合のみ使う

  Obj *var;      // Used if kind == ND_VAR
};


// Function
typedef struct Function Function;
struct Function {
  Function *next;
  char *name;
  Node *body;
  Obj *locals;
  int stack_size;
};


Function *parse(Token *tok);

//
// type.c
//
typedef enum {
  TY_INT, // int型
  TY_PTR, // ポインタ型
  TY_FUNC, // 関数型
} TypeKind;

struct Type {
  TypeKind kind;

  // Pointer
  Type *base;

  // Declaration
  Token *name;

  // Function type
  Type *return_ty;
};

extern Type *ty_int;

bool is_integer(Type *ty);
Type *pointer_to(Type *base);
Type *func_type(Type *return_ty);
void add_type(Node *node);


//
// codegen.c
//
void codegen(Function *prog);
