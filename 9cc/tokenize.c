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

void error_tok(Token *tok, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int pos = tok->loc - current_input;
  fprintf(stderr, "%s\n", current_input);
  fprintf(stderr, "%*s", pos, ""); // print pos spaces.
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

bool startswith(char *p, char *q) {
  return memcmp(p, q, strlen(q)) == 0;
}

int from_hex(char c) {
  if ('0' <= c && c <= '9') {
    return c - '0';
  }
  if ('a' <= c && c <= 'f') {
    return c - 'a' + 10;
  }
  return c - 'A' + 10;
}

// Read a punctuator token from p and returns its length.
int read_punct(char *p) {
  if (startswith(p, "==") || startswith(p, "!=") ||
      startswith(p, "<=") || startswith(p, ">="))
    return 2;

  return ispunct(*p) ? 1 : 0;
}

int is_alnum(char c) {
  return isalnum(c) || c == '_';
}

bool is_keyword(Token *tok) {
  char *kw[] = {"return", "if", "else", "for", "while", "int", "sizeof"};

  for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++) {
    if (startswith(tok->loc, kw[i]) && !is_alnum(tok->loc[strlen(kw[i])])) {
      return true;
    }
  }
  return false;
}

// 新しいトークンを作成してcurに繋げる
Token *new_token(TokenKind kind, Token *cur, char *loc, int len) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->loc = loc;
  tok->len = len;
  cur->next = tok;
  return tok;
}

int read_escaped_char(char **new_pos, char *p) {
  if ('0' <= *p && *p <= '7') {
    // Read an octal number.
    int c = *p++ - '0';
    if ('0' <= *p && *p <= '7') {
      c = (c << 3) + (*p++ - '0');
      if ('0' <= *p && *p <= '7')
        c = (c << 3) + (*p++ - '0');
    }
    *new_pos = p;
    return c;
  }

  if (*p == 'x') {
    // Read a hexadecimal number.
    p++;
    if (!isxdigit(*p)) {
      error_at(p, "invalid hex escape sequence");
    }

    int c = 0;
    for (; isxdigit(*p); p++) {
      c = (c << 4) + from_hex(*p);
    }
    *new_pos = p;
    return c;
  }

  *new_pos = p + 1;

  switch (*p) {
  case 'a': return '\a';
  case 'b': return '\b';
  case 't': return '\t';
  case 'n': return '\n';
  case 'v': return '\v';
  case 'f': return '\f';
  case 'r': return '\r';
  // [GNU] \e for the ASCII escape character is a GNU C extension.
  case 'e': return 27;
  default: return *p;
  }
}

// Find a closing double-quote.
char *string_literal_end(char *p) {
  char *start = p;
  for (; *p != '"'; p++) {
    if (*p == '\n' || *p == '\0')
      error_at(start, "unclosed string literal");
    if (*p == '\\')
      p++;
  }
  return p;
}

Token *read_string_literal(char *start, Token *cur) {
  char *end = string_literal_end(start + 1);
  char *buf = calloc(1, end - start);
  int len = 0;

  for (char *p = start + 1; p < end;) {
    if (*p == '\\') {
      buf[len++] = read_escaped_char(&p, p + 1);
    } else {
      buf[len++] = *p++;
    }
  }

  Token *token = new_token(TK_STR, cur, start, end + 1 - start);
  token->ty = array_of(ty_char, len + 1);
  token->str = buf;
  return token;
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

    // 変数・キーワード（複数文字対応）
    if (isalpha(*p) || *p == '_') {
      char *start = p;
      while (isalnum(*p) || *p == '_')
        p++;
      cur = new_token(TK_IDENT, cur, start, p - start);
      if (is_keyword(cur))
        cur->kind = TK_KEYWORD;
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

    // 文字列リテラル
    if (*p == '"') {
      cur = cur->next = read_string_literal(p, cur);
      p += cur->len;
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