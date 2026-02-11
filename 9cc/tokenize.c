#include "9cc.h"

// Input string
static char *current_input;

// エラーを報告するための関数
// printfと同じ引数を取る
void error(char *fmt, ...) {
  va_list ap;     // va_listは可変長引数を扱うchar型のポインタ
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// エラー箇所を報告する
void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int pos = loc - current_input;
  fprintf(stderr, "%s\n", current_input);
  fprintf(stderr, "%*s", pos, " "); // pos個の空白を出力
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

bool startswith(char *p, char *q) {
  return memcmp(p, q, strlen(q)) == 0;
}

// Read a punctuator token from p and returns its length.
static int read_punct(char *p) {
  if (startswith(p, "==") || startswith(p, "!=") ||
      startswith(p, "<=") || startswith(p, ">="))
    return 2;

  return ispunct(*p) ? 1 : 0;
}

int is_alnum(char c) {
  return isalnum(c) || c == '_';
}

// 新しいトークンを作成してcurに繋げる
Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
  cur->next = tok;
  return tok;
}

// 入力文字列pをトークナイズしてそれを返す
Token *tokenize(char *p) {
  current_input = p;
  Token head;
  head.next = NULL;
  Token *cur = &head;

  while (*p) {
    // 空白文字をスキップ
    if (isspace(*p)) {
      p++;
      continue;
    }

    if (startswith(p, "return") && !is_alnum(p[6])) {
      cur = new_token(TK_KEYWORD, cur, p, 6);
      p += 6;
      continue;
    }

    // 変数（複数文字対応）
    if (isalpha(*p) || *p == '_') {
      char *start = p;
      while (isalnum(*p) || *p == '_')
        p++;
      cur = new_token(TK_IDENT, cur, start, p - start);
      continue;
    }

    // 数字
    if (isdigit(*p)) {
      cur = new_token(TK_NUM, cur, p, 0);
      char *q = p;
      cur->val = strtol(p, &p, 10);
      cur->len = p - q; // トークンの長さを設定
      continue;
    }

    // 単数文字の演算子
    // 複数文字の演算子, 単数文字の演算子, 区切り文字(現状セミコロン)
    int punct_len = read_punct(p);
    if (punct_len) {
      cur = cur->next = new_token(TK_PUNCT, cur, p, punct_len);
      p += cur->len;
      continue;
    }

    error_at(p, "invalid token");
  }

  new_token(TK_EOF, cur, p, 0);
  return head.next;
}