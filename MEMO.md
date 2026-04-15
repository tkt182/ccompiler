# parse.c 解説

## 概要

`tokenize.c` が生成したトークン列を受け取り、**抽象構文木（AST）** を構築して返す。  
手法は**再帰下降パーサ**。エントリポイントは `parse(Token *token)`。

---

## 全体のデータフロー

```
tokenize.c          parse.c                    codegen.c
  Token列  ───────▶  AST (Node の連結リスト)  ───▶  アセンブリ出力
```

---

## トークンの種類（`TokenKind`）

| 種類 | 説明 | 例 |
|---|---|---|
| `TK_IDENT` | 識別子（変数名・関数名） | `main`, `foo` |
| `TK_PUNCT` | 記号・演算子 | `+`, `(`, `==` |
| `TK_KEYWORD` | キーワード | `return`, `if`, `else`, `for`, `while` |
| `TK_NUM` | 整数リテラル | `42`, `0` |
| `TK_EOF` | 入力の終端 | |

> `TK_KEYWORD` は `TK_IDENT` として読み込んだあと `is_keyword()` で判定してkindを書き換える。

---

## ノードの種類（`NodeKind`）

### 式ノード
| 種類 | 説明 | 使用フィールド |
|---|---|---|
| `ND_NUM` | 整数リテラル | `val` |
| `ND_VAR` | 変数参照 | `var` |
| `ND_NEG` | 単項マイナス | `lhs` |
| `ND_ADD` | 加算 | `lhs`, `rhs` |
| `ND_SUB` | 減算 | `lhs`, `rhs` |
| `ND_MUL` | 乗算 | `lhs`, `rhs` |
| `ND_DIV` | 除算 | `lhs`, `rhs` |
| `ND_ASSIGN` | 代入 (`=`) | `lhs`, `rhs` |
| `ND_EQ` | 等値 (`==`) | `lhs`, `rhs` |
| `ND_NE` | 非等値 (`!=`) | `lhs`, `rhs` |
| `ND_LT` | 未満 (`<`) | `lhs`, `rhs` |
| `ND_LE` | 以下 (`<=`) | `lhs`, `rhs` |
| `ND_FUNCALL` | 関数呼び出し | `funcname`, `args` |

### 文ノード
| 種類 | 説明 | 使用フィールド |
|---|---|---|
| `ND_EXPR_STMT` | 式文（結果を捨てる） | `lhs` |
| `ND_NULL_STMT` | 空文 (`;` のみ) | なし |
| `ND_RETURN` | return 文 | `lhs` |
| `ND_IF` | if / if-else 文 | `cond`, `then`, `els` |
| `ND_FOR` | for 文・while 文 | `init`, `cond`, `inc`, `then` |
| `ND_BLOCK` | ブロック `{ ... }` | `body`（連結リスト）|

---

## 関数の構成

### [1] ユーティリティ

トークン列を操作する基本道具。

| 関数 | 役割 |
|---|---|
| `equal` | 現在のトークンが指定文字列と一致するか確認（読み進めない） |
| `skip` | 一致したら読み進める。不一致ならエラー |
| `consume` | 一致したら読み進めて `true`。不一致なら `false`（エラーなし） |
| `expect` | 一致したら読み進める。不一致ならエラー（記号専用） |
| `expect_number` | 数値トークンを読み進めて値を返す |
| `at_eof` | 終端トークンか確認 |

`Token **rest` という引数パターンが重要で、**「どこまで読んだか」** を呼び出し元に返すための慣用句。

---

### [2] ノードコンストラクタ

AST のノードを生成するヘルパー群。

| 関数 | 生成するノード |
|---|---|
| `new_node` | 汎用ノード（種類と代表トークンのみ） |
| `new_num` | 整数リテラルノード（トークン情報あり） |
| `new_binary` | `lhs OP rhs` の二項演算ノード |
| `new_unary` | 単項演算ノード（`ND_NEG` で使用。`lhs` のみ設定、`rhs = NULL`） |
| `new_var_node` | 変数参照ノード。`var` に `Obj *` をセット |

---

### [3] 変数・型ユーティリティ

変数の登録・検索を担う。

| 関数 | 役割 |
|---|---|
| `get_ident` | トークンが識別子なら名前文字列を返す |
| `new_lvar` | 新しいローカル変数を `locals` リストの先頭に追加。`offset` は `Obj` 側で自動計算 |
| `consume_ident` | 識別子トークンなら読み進めてそのトークンを返す |
| `find_var` | `locals` リストを線形探索して変数を返す |

変数のオフセット計算（rbp からのバイト距離）:

```
1個目の変数 → rbp - 8
2個目の変数 → rbp - 16
3個目の変数 → rbp - 24  ...
```

---

### [4] 型パーサ

現在は `int` 型と関数型のみ対応。

| 関数 | 役割 |
|---|---|
| `declspec` | `"int"` を消費して `ty_int` を返す |
| `type_suffix` | `(` が続けば関数型に変換 |
| `declarator` | `*` があればポインタ型。識別子名を `ty->name` に記録 |

---

### [5] 式パーサ

**演算子の優先順位を関数の呼び出し階層で表現**するのが再帰下降の核心。  
優先度が低いものほど上位の関数になる。

```
assign        ← 最低優先（= による代入）
  └─ equality       （==, !=）
       └─ relational    （<, <=, >, >=）
            └─ add           （+, -）
                 └─ mul          （*, /）
                      └─ unary       （単項 +/-）
                           └─ primary     （最高優先：数値・変数・括弧）
```

#### assign
`equality()` を呼ぶ前に `識別子 + "="` を先読みして未登録変数を自動登録する（暗黙的な変数宣言）。

#### new_add / new_sub
ポインタ演算を考慮する。`ptr + num` の場合は `num * 8` に自動変換して要素単位のアドレス計算にする。

#### unary
`-x` は `ND_NEG` ノード（単項マイナス専用）として展開する。  
`new_unary(ND_NEG, expr, tok)` で `lhs` のみを持つノードを生成し、`codegen.c` 側で `neg rax` 命令1つに変換する。

> **注意**: `new_unary(ND_SUB, ...)` を使うと `rhs = NULL` になり、二項演算パス `codegen(node->rhs)` で Segfault する。  
> `ND_NEG` を使う場合は `type.c` の `add_type` にも対応を追加すること（`node->ty = node->lhs->ty`）。

#### primary
式の末端（葉ノード）を処理する:
1. 識別子 → 関数呼び出し (`funcall`) または変数参照
2. `(` → 括弧式
3. 数値リテラル

---

### [6] 文パーサ

`stmt` が各文の種類を先頭トークンで判定する。

| 先頭 | 生成するノード | 備考 |
|---|---|---|
| `return` | `ND_RETURN` | `lhs` に式 |
| `if` | `ND_IF` | `cond` / `then` / `els` |
| `for` | `ND_FOR` | `init` / `cond` / `inc` / `then` |
| `while` | `ND_FOR` で代用 | `init` / `inc` が NULL |
| `{` | `compound_stmt` へ委譲 | |
| その他 | `expr_stmt`（式文） | |

`compound_stmt` は `{ stmt* }` を処理し、文を `ND_BLOCK` ノードの `body` に連結リストとして繋ぐ。

---

### [7] トップレベル

```
parse()
  └─ function()  ← EOF までループで関数定義を1つずつ処理
                    Function の連結リストにして返す
```

#### function()
`declspec` → `declarator` の順に型と関数名をパースし、`compound_stmt` で本体を処理する。
- `fn->name` は `declarator` が `ty->name` にセットしたトークンから `get_ident()` で取得する
- `compound_stmt` 自身が `{` を消費するため、呼び出し側で `{` を skip してはいけない

この `Function` の連結リストが `main.c` → `codegen.c` に渡される。

---

## codegen.c 解説

### アセンブリ構文
**Intel 構文**（`.intel_syntax noprefix`）を使用。

| 項目 | Intel 構文（本実装） | AT&T 構文（chibicc参考） |
|---|---|---|
| レジスタ | `rax` | `%rax` |
| メモリ参照 | `[rax]` | `(rax)` |
| 即値 | `0` | `$0` |
| オペランド順 | `dst, src` | `src, dst` |

---

### 式の評価方式：rax 方式

`gen_expr` は評価結果を **`rax` レジスタに残す**。スタックへの push は行わない。  
`push`/`pop` は二項演算・代入・関数呼び出しで一方の値を**一時退避**するためだけに使う。

```
gen_expr(rhs)  → rax = rhs の値
push()         → rhs を退避
gen_expr(lhs)  → rax = lhs の値
pop(rdi)       → rdi = rhs の値
add rax, rdi   → rax = lhs + rhs
```

`label_count` はスタック深度のトラッキングに使用（`push` で +1、`pop` で -1）。

---

### gen_expr の各ケース

| ケース | 処理 |
|---|---|
| `ND_NUM` | `mov rax, <値>` |
| `ND_NEG` | lhs を評価後 `neg rax` |
| `ND_VAR` | `codegen_lval` でアドレスを rax に取得 → `mov rax, [rax]` で値を読み込む |
| `ND_ASSIGN` | lhs のアドレスを push で退避 → rhs を評価 → pop で rdi にアドレス取得 → `mov [rdi], rax` |
| `ND_FUNCALL` | 各引数を `gen_expr` + `push` で積む → 逆順に `pop` して引数レジスタ(rdi,rsi...)へ → `call` |
| 二項演算 | rhs → push → lhs → pop(rdi) → 演算（結果は rax）|

#### codegen_lval
変数のアドレスを **rax に返す**（push しない）。
```
mov rax, rbp
sub rax, <offset>   ← rax = 変数のアドレス
```

---

### gen_stmt の各ケース

| ケース | 処理 |
|---|---|
| `ND_BLOCK` | `body` の各ノードを順に `gen_stmt` |
| `ND_NULL_STMT` | 何もしない |
| `ND_EXPR_STMT` | `gen_expr(node->lhs)` のみ（結果は捨てる）|
| `ND_RETURN` | `gen_expr` → `jmp .L.return.<関数名>` |
| `ND_IF` | `gen_expr(cond)` → `cmp rax, 0` → 条件分岐ラベル |
| `ND_FOR` | init → begin ラベル → cond → body → inc → jmp begin |

---

### codegen のフロー

```
codegen(Function *prog)
  ├─ .intel_syntax noprefix
  └─ 各 Function ごとに
       ├─ プロローグ: push rbp / mov rbp, rsp / sub rsp, 208
       ├─ gen_stmt(fn->body)
       └─ エピローグ: .L.return.<name>: / mov rsp, rbp / pop rbp / ret
```

---

## type.c 解説

型システムを管理するファイル。

### 型の種類（`TypeKind`）

| 種類 | 説明 |
|---|---|
| `TY_INT` | int 型 |
| `TY_PTR` | ポインタ型（`base` が指す型へのポインタ）|
| `TY_FUNC` | 関数型 |

### 主な関数

| 関数 | 役割 |
|---|---|
| `is_integer` | `TY_INT` かどうか確認 |
| `pointer_to(base)` | `base` へのポインタ型を生成 |
| `func_type(return_ty)` | 関数型を生成 |
| `add_type(node)` | AST を再帰的に走査して各ノードに型をセット |

`add_type` は `new_add` / `new_sub` など型に依存する演算の前に呼ぶ必要がある。
