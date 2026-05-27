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
| `TK_STR` | 文字列リテラル | `"abc"`, `""` |
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
| `ND_NEG` | 単項マイナス | `lhs` || `ND_ADDR` | アドレス取得 (`&`) | `lhs` |
| `ND_DEREF` | デリファレンス (`*`) | `lhs` || `ND_ADD` | 加算 | `lhs`, `rhs` |
| `ND_SUB` | 減算 | `lhs`, `rhs` |
| `ND_MUL` | 乗算 | `lhs`, `rhs` |
| `ND_DIV` | 除算 | `lhs`, `rhs` |
| `ND_ASSIGN` | 代入 (`=`) | `lhs`, `rhs` |
| `ND_EQ` | 等値 (`==`) | `lhs`, `rhs` |
| `ND_NE` | 非等値 (`!=`) | `lhs`, `rhs` |
| `ND_LT` | 未満 (`<`) | `lhs`, `rhs` |
| `ND_LE` | 以下 (`<=`) | `lhs`, `rhs` |
| `ND_FUNCALL` | 関数呼び出し | `funcname`, `args` |
| `ND_STMT_EXPR` | 文式 `({ ... })` (GNU C拡張) | `body` |

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
| `new_var` | `Obj` を生成する基底関数。`push_scope` を呼んで現在のスコープに変数を登録する |
| `new_lvar` | 新しいローカル変数を `locals` リストの先頭に追加 |
| `new_gvar` | 新しいグローバル変数・関数を `globals` リストの先頭に追加 |
| `new_unique_name` | `format()` を使い `.L..0`, `.L..1` ... のような一意な名前を生成する |
| `new_anon_gvar` | 一意な名前で匿名グローバル変数を生成する |
| `new_string_literal` | 文字列リテラル用の匿名グローバル変数を生成し `init_data` をセットする |
| `consume_ident` | 識別子トークンなら読み進めてそのトークンを返す |
| `push_scope` | 現在のスコープ（`scope->vars`）の先頭に `VarScope` を追加し、名前と `Obj *` を登録する |
| `enter_scope` | 新しい `Scope` を生成してスタックに積む（ブロック `{` の開始時） |
| `leave_scope` | `scope = scope->next` でスコープを1段抜ける（ブロック `}` の終了時） |
| `find_var` | `scope` チェーンを外側に向かって線形探索し、最も内側で一致した変数を返す |

変数のオフセット計算（rbp からのバイト距離）:

```
1個目の変数（int: 8byte）  → rbp - 8
2個目の変数（int: 8byte）  → rbp - 16
3個目の変数（int[3]: 24byte）→ rbp - 40  ...
```

オフセットは `ty->size` を加算することで、配列など任意サイズの型に対応する。

---

### [4] 型パーサ

現在は `char` 型、`int` 型、ポインタ型、配列型、関数型に対応。

| 関数 | 役割 |
|---|---|
| `declspec` | `"char"` または `"int"` を消費して対応する型を返す |
| `is_typename` | 現在のトークンが型名（`"char"` または `"int"`）かどうかを判定 |
| `func_params` | `)` まで引数型リストをパースして関数型を返す |
| `type_suffix` | `(` が続けば関数型、`[N]` が続けば**再帰的に**配列型をネスト、それ以外はそのまま |
| `declarator` | `*` の数だけポインタ型をラップ。識別子名を `ty->name` に記録 |

配列型 `int x[N]` のパース:
```
declspec → ty_int
declarator → type_suffix で "[N]" を検出 → array_of(ty_int, N) を返す
```

多次元配列 `int x[2][3]` のパース（`type_suffix` の再帰）:
```
type_suffix(token=[2][3], ty=TY_INT)
  └─ [2] を読み取り、再帰: type_suffix(token=[3], ty=TY_INT)
       └─ [3] を読み取り、再帰: type_suffix(token=ε, ty=TY_INT)
            └─ TY_INT をそのまま返す
       └─ array_of(TY_INT, 3) を返す
  └─ array_of(array_of(TY_INT, 3), 2) を返す
```

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
                      └─ unary       （単項 +/-/*/& / sizeof）
                           └─ postfix    （後置 []）
                                └─ primary     （最高優先：数値・変数・括弧）
```

#### assign
`equality()` を呼ぶ前に `識別子 + "="` を先読みして未登録変数を自動登録する（暗黙的な変数宣言）。

#### declaration
`int` キーワードで始まる変数宣言文を処理する。`compound_stmt` が `int` トークンを先読みして `stmt` の代わりに呼ぶ。

- `int x;` → `Obj` を生成してローカル変数リストに追加
- `int x = 3;` → 変数生成 + `ND_ASSIGN` ノードを `ND_EXPR_STMT` でラップして返す
- `int x[3];` → `array_of(ty, 3)` で配列型を生成し、必要なサイズ（24byte）分オフセットを確保

#### new_add / new_sub
ポインタ・配列演算を考慮する。`ptr + num` の場合は `num * base->size` に自動変換して要素単位のアドレス計算にする（固定値 `8` ではなく `ty->base->size` を使う）。

#### unary
`-x` は `ND_NEG` ノード（単項マイナス専用）として展開する。  
`&x` は `ND_ADDR` ノード、`*x` は `ND_DEREF` ノードとして展開する。  
前置演算子がない場合は `postfix` に委譲する。

#### postfix
`primary` の直後に `[` が続く場合に添字アクセスを処理する。  
`x[i]` は `*(x+i)` と等価なため、`ND_DEREF(new_add(x, i))` のノードに変換する（`new_add` がポインタ演算のサイズスケーリングも処理する）。  
`while` ループで `x[1][2]` のような連続した添字も処理できる。

後置演算子 `[]` は前置単項演算子 `*`, `&` より優先度が高いため（`*x[1]` → `*(x[1])`）、`unary` → `postfix` → `primary` という呼び出し階層で表現する。

#### primary
式の末端（葉ノード）を処理する:
1. `( {` → **文式**（GNU C拡張）: `compound_stmt` でブロックをパースし `ND_STMT_EXPR` ノードを返す
2. `(` → 括弧式
3. `sizeof` → オペランドを `unary` で取得して型を確定させ、`ty->size` を持つ `ND_NUM` ノードを返す（オペランド自体はASTに組み込まず捨てる）
4. 識別子 → 関数呼び出し (`funcall`) または変数参照
5. 文字列リテラル → `new_string_literal` で匿名グローバル変数を生成し `ND_VAR` ノードを返す
6. 数値リテラル

```
primary = "(" "{" stmt+ "}" ")"
        | "(" expr ")"
        | sizeof unary
        | ident func-args?
        | str
        | num
```

`sizeof` はコンパイル時定数なので、コード生成の変更は不要。`sizeof(x=2)` のようにオペランドに副作用がある式を書いても実行されない。

文式 `({ stmt... })` はブロックの最後の式の値が文式全体の値になる（GNU C拡張）。

```c
int x = ({ int a = 1; int b = 2; a + b; }); // x == 3
```

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
先頭が型名（`is_typename` で判定）なら `declaration`、それ以外は `stmt` として処理する。各文の処理後に `add_type` を呼んで型情報を付与する。  
ブロックの開始・終了時に `enter_scope()` / `leave_scope()` を呼んでスコープを管理する（ブロック内で宣言した変数は外側から見えなくなる）。

---

### [7] トップレベル

```
parse()
  ├─ is_function() で関数かグローバル変数かを判定
  ├─ function()         ← 関数定義を処理し globals に追加
  └─ global_variable()  ← グローバル変数宣言を処理し globals に追加
```

`parse` は `globals`（`Obj` の連結リスト）を返す。関数・グローバル変数ともに `globals` に入り、`is_function` フラグで区別する。

#### function()
`declspec` → `declarator` の順に型と関数名をパースし、`compound_stmt` で本体を処理する。
- `new_gvar` で `globals` に追加し、`is_function = true` をセット
- `fn->locals` に関数内のローカル変数リストを保存
- `enter_scope()` で関数スコープを開始し、引数変数を登録した後 `compound_stmt` を呼ぶ。終了後 `leave_scope()`
- 処理済みの `Token *` を返す（呼び出し元がトークン位置を進めるため）

#### global_variable()
`int x, y;` のようなグローバル変数宣言を処理する。`,` 区切りで複数変数を `new_gvar` で `globals` に追加する。  
初期化子 `=` がある場合は `gvar_initializer` を呼んで `Obj.init_data` を構築する。

グローバル初期化子の実装ポイント:
- `gvar_init_scalar`: 数値を1要素分のバイト列として `buf` に書き込む
- `gvar_init_array`: `{...}` または文字列リテラルで配列を初期化する
- `gvar_initializer`: 型ごとに分岐して初期化データを作る

`gvar_initializer` の主な分岐:
- `TY_PTR` + `TK_STR`:
  文字列リテラルを匿名グローバルに置き、`reloc_label` にそのラベル名を保存する
- `TY_PTR` + `"&" ident`:
  グローバル変数のアドレスを初期化子として受理し、`reloc_label` を参照先シンボル名に設定する
- `TY_PTR` + `ident ("+" num | "-" num)?`:
  `char *c = msg1 + 1` のような初期化を受理し、`reloc_label` と `reloc_addend`（バイト単位）を設定する
- `TY_ARRAY` + `array_len==0` + `TK_STR`:
  `char s[] = "abc"` のような不完全配列を、文字列長（終端 `\0` を含む）で確定する
- それ以外:
  `calloc` で `init_data` を確保し、スカラー/配列の初期化子をバイト列に変換する

補足:
- `type_suffix` は `[]`（要素数省略）を受理し、要素数 `0` の不完全配列として型を作る
- その後 `gvar_initializer` が実初期化子を見てサイズを確定する

#### is_function()
`declarator` を試し呼びして型が `TY_FUNC` かどうかで関数/変数を判定する。

この `globals` リストが `main.c` → `codegen.c` に渡される。

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
| `ND_VAR` | `gen_addr` でアドレスを rax に取得 → `load` で値を読み込む（配列型はアドレスのまま） |
| `ND_DEREF` | lhs を評価（ポインタ値を rax に）→ `load` でデリファレンス |
| `ND_ADDR` | `gen_addr(lhs)` でアドレスだけを rax に返す（デリファレンスしない） |
| `ND_ASSIGN` | `gen_addr(lhs)` でアドレスを push で退避 → rhs を評価 → `store` で書き込み |
| `ND_STMT_EXPR` | `body` の各ノードを順に `gen_stmt`（最後の `ND_EXPR_STMT` の結果が `rax` に残る） |
| `ND_FUNCALL` | 各引数を `gen_expr` + `push rax` で積む → 逆順に `pop argreg64` して引数レジスタへ → `call` |
| 二項演算 | rhs → push → lhs → pop(rdi) → 演算（結果は rax）|

#### gen_addr
変数・デリファレンス式の **アドレスを rax に返す**（値は読まない）。

| ケース | 処理 |
|---|---|
| `ND_VAR`（ローカル） | `lea rax, [rbp - offset]` |
| `ND_VAR`（グローバル） | `lea rax, [rip + 変数名]`（RIP相対アドレッシング） |
| `ND_DEREF` | `gen_expr(lhs)`（ポインタの値 = 指すアドレスをそのまま rax に） |

#### load
`rax` が指すアドレスから値を読み込み `rax` に上書きする。

| 型 | 命令 | 説明 |
|---|---|---|
| `TY_ARRAY` | （スキップ） | 配列はアドレスをそのまま使う |
| `TY_CHAR`（size=1） | `movzx rax, byte ptr [rax]` | 1バイト読み込み。上位56ビットをゼロ拡張して `rax` 全体を更新 |
| その他（size=8） | `mov rax, [rax]` | 8バイト読み込み |

`byte ptr` はメモリから読むバイト数（1バイト）を指定するサイズ指定子。  
x86-64では `al`（下位8ビット）への書き込みは上位56ビットが変化しないため、`movzx` でゼロ拡張が必要。

#### store
スタックから左辺のアドレスを `rdi` に取り出し、`rax` の値を書き込む。

| 型 | 命令 | 説明 |
|---|---|---|
| `TY_CHAR`（size=1） | `mov [rdi], al` | `rax` の下位8ビットのみ1バイト書き込む |
| その他（size=8） | `mov [rdi], rax` | 8バイト書き込む |

#### 引数レジスタ

| 用途 | レジスタ | 説明 |
|---|---|---|
| 呼び出し元（`ND_FUNCALL`） | `argreg64`（`rdi`,`rsi`,...） | ABIの規約で常に64ビットレジスタで渡す |
| 呼び出し先（`emit_text`）`char` | `argreg8`（`dil`,`sil`,...） | 下位8ビット = `argreg64` の下位8ビット |
| 呼び出し先（`emit_text`）その他 | `argreg64`（`rdi`,`rsi`,...） | 8バイトでスタックに保存 |


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

### println

`codegen.c` 全体で使う出力ヘルパー。`printf` スタイルで書いた後に自動で改行を追加する。出力先は `output_file`（モジュール変数）で、`codegen()` 呼び出し時に設定される。

```c
void println(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(output_file, fmt, ap);
  va_end(ap);
  fprintf(output_file, "\n");
}
```

すべての `printf` 呼び出しをこの関数に置き換えることで、出力先の切り替えを一箇所で管理する。

### codegen のフロー

```
codegen(Obj *prog, FILE *out)
  ├─ output_file = out  ← 出力先をモジュール変数にセット
  ├─ .intel_syntax noprefix
  ├─ assign_lvar_offsets() ← 各関数のローカル変数にオフセットを割り当て
  ├─ emit_data()           ← グローバル変数を .data セクションに出力
  └─ emit_text()           ← 各関数を .text セクションに出力
       ├─ プロローグ: push rbp / mov rbp, rsp / sub rsp, <stack_size>
       ├─ gen_stmt(fn->body)
       └─ エピローグ: .L.return.<name>: / mov rsp, rbp / pop rbp / ret
```

#### assign_lvar_offsets
各関数の `locals` リストを走査し、`var->offset` を `ty->size` の累積値として設定する。`stack_size` は `align_to(offset, 16)` で16バイトアライン（x86-64 ABI要件）。

#### emit_data
`is_function = false` の `Obj`（グローバル変数）を `.data` セクションに出力する。

| 条件 | 出力 | 用途 |
|---|---|---|
| `init_data` あり（`reloc_label` なし） | `.byte <値>` を `size` 回繰り返す | 数値/配列/文字列の固定初期化 |
| `init_data` あり（`reloc_label` あり） | 先頭を `.quad <label>` または `.quad <label>+<addend>` / `.quad <label><addend>` で出力し、残りは `.byte` | `char *p = "foo"`, `int *g6 = &g3`, `char *c = msg1 + 1` のようなアドレス再配置が必要な初期化 |
| `init_data` なし | `.zero <size>` | 通常のグローバル変数（ゼロ初期化） |

文字列リテラル `"abc"` の場合、`init_data` は `{'a','b','c','\0'}` なので:
```asm
.L..0:
  .byte 97   ; 'a'
  .byte 98   ; 'b'
  .byte 99   ; 'c'
  .byte 0    ; '\0'
```

#### emit_text
`is_function = true` の `Obj`（関数）を `.text` セクションに出力する。各関数の前に `.text` を出力することで、`emit_data` 後もコードセクションに正しく切り替わる。

---

## type.c 解説

型システムを管理するファイル。

### 型の種類（`TypeKind`）

| 種類 | 説明 |
|---|---|
| `TY_CHAR` | char 型（size = 1）|
| `TY_INT` | int 型（size = 8）|
| `TY_PTR` | ポインタ型（`base` が指す型へのポインタ、size = 8）|
| `TY_FUNC` | 関数型 |
| `TY_ARRAY` | 配列型（`base` が要素型、`array_len` が要素数、size = base->size * len）|

### 主な関数

| 関数 | 役割 |
|---|---|
| `is_integer` | `TY_CHAR` または `TY_INT` かどうか確認 |
| `pointer_to(base)` | `base` へのポインタ型を生成（size = 8） |
| `func_type(return_ty)` | 関数型を生成 |
| `array_of(base, len)` | `base` 型の要素を `len` 個持つ配列型を生成（size = base->size * len） |
| `add_type(node)` | AST を再帰的に走査して各ノードに型をセット |

#### ND_STMT_EXPR の型推論

`body` の連結リストを末尾まで走査し、最後のノードが `ND_EXPR_STMT` であればその `lhs->ty` を文式全体の型とする。最後のノードが `ND_EXPR_STMT` でない場合（void を返す場合）はコンパイルエラー。

```c
// body リストの末尾を探す
Node *stmt = node->body;
while (stmt->next) stmt = stmt->next;
// 最後が式文なら型を継承
if (stmt->kind == ND_EXPR_STMT)
  node->ty = stmt->lhs->ty;
else
  error_tok(node->tok, "statement expression returning void is not supported");
```

---

## strings.c 解説

フォーマット文字列を受け取り、ヒープに確保した文字列を返すユーティリティ関数。

### format()

`printf` スタイルのフォーマット文字列を受け取り、整形済み文字列を返す。  
`open_memstream` を使うことでバッファサイズを事前に決める必要がなく、任意長の文字列を安全に生成できる。

```c
char *format(char *fmt, ...) {
  char *buf;
  size_t buflen;
  FILE *out = open_memstream(&buf, &buflen); // 動的バッファに書き込むストリームを開く

  va_list ap;
  va_start(ap, fmt);
  vfprintf(out, fmt, ap);
  va_end(ap);
  fclose(out); // fclose 時に buf が確定しヌル終端される
  return buf;  // 呼び出し元が所有権を持つ（free が必要）
}
```

`calloc(1, 20) + sprintf` のような固定サイズバッファを不要にし、`new_unique_name()` などから利用される。

---

## tokenize.c 解説

ファイルを読み込み、`Token` の連結リストに変換する。公開エントリポイントは `tokenize_file(char *path)`。

### 主な関数

| 関数 | 役割 |
|---|---|
| `new_token` | `Token` を生成してリストに繋ぐ基底関数 |
| `from_hex` | 16進数文字（`0`〜`9`, `a`〜`f`, `A`〜`F`）を対応する整数値に変換する |
| `read_punct` | `==`, `!=`, `<=`, `>=` などの複数文字演算子を判定して長さを返す |
| `is_keyword` | 識別子がキーワードかどうか確認。該当する場合は `TK_IDENT` → `TK_KEYWORD` に書き換える |
| `read_escaped_char` | `\n`, `\t` などのエスケープ文字、`\0`, `\101` などの8進数エスケープ、`\x41` などの16進数エスケープを対応するASCII値に変換する |
| `string_literal_end` | 閉じ `"` の位置を返す（`\"` はスキップ） |
| `read_string_literal` | `"..."` を読み取りエスケープ処理済みの `TK_STR` トークンを生成する |
| `verror_at` | エラー箇所をファイル名・行番号付きで `ファイル名:行番号: ソース行 ^ メッセージ` の形式で表示して終了する |
| `read_file` | ファイルを読み込んで文字列として返す。パスが `"-"` の場合は標準入力から読む |
| `tokenize` | ファイル名と入力文字列を受け取り、先頭から走査して `Token` リストを構築する |
| `tokenize_file` | `read_file` でファイルを読み込んで `tokenize` を呼ぶ公開API |

### エラーメッセージ

`current_filename` と `current_input` の2つのモジュール変数でエラー表示に必要な情報を保持する。`verror_at` は `loc` からその行の先頭を逆走査して行番号を算出し、以下の形式で表示する:

```
foo.c:3: int x = 1 + ;
                    ^ expected an expression
```

### コメントスキップ（`tokenize` 内）

| 種類 | 構文 | 処理 |
|---|---|---|
| 行コメント | `// ...` まで行末 | `//` を検出したら `\n` まで `p` を進めて `continue` |
| ブロックコメント | `/* ... */` | `/*` を検出したら `strstr` で `*/` を探し、見つかれば `p` を `*/` の直後に進める。見つからなければ `error_at` |

### read_escaped_char

`\` の直後の文字を受け取り、対応するASCII値を返す。読み終えた次の位置を `*new_pos` に書き込む。

8進数・16進数エスケープは**可変長**のため、何文字消費したかを呼び出し元に伝える必要がある。そのため引数に `char **new_pos` を追加し、名前付きエスケープも含めて統一的に次の位置を返す設計になっている。

```c
int read_escaped_char(char **new_pos, char *p) {
  if ('0' <= *p && *p <= '7') {
    // 8進数エスケープ（最大3桁）
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
    // 16進数エスケープ（桁数制限なし）
    p++;
    if (!isxdigit(*p))
      error_at(p, "invalid hex escape sequence");
    int c = 0;
    for (; isxdigit(*p); p++)
      c = (c << 4) + from_hex(*p);
    *new_pos = p;
    return c;
  }

  *new_pos = p + 1;

  switch (*p) {
  case 'a': return '\a';  // 7  : ベル
  case 'b': return '\b';  // 8  : バックスペース
  case 't': return '\t';  // 9  : タブ
  case 'n': return '\n';  // 10 : 改行
  case 'v': return '\v';  // 11 : 垂直タブ
  case 'f': return '\f';  // 12 : フォームフィード
  case 'r': return '\r';  // 13 : キャリッジリターン
  case 'e': return 27;    // 27 : ESC（GNU拡張）
  default:  return *p;    // 未知のエスケープはそのまま返す（例: \j → 'j'）
  }
}
```

8進数エスケープの例：

| リテラル | 解釈 | 値 |
|---|---|---|
| `"\0"` | 8進数 0 | 0（NUL） |
| `"\20"` | 8進数 20 | 16 |
| `"\101"` | 8進数 101 | 65（= `'A'`） |
| `"\1500"` | 8進数 150（3桁まで） + `'0'` | 104 と 48 の2文字 |

16進数エスケープの例：

| リテラル | 解釈 | 値 |
|---|---|---|
| `"\x00"` | 16進数 00 | 0（NUL） |
| `"\x77"` | 16進数 77 | 119（= `'w'`） |
| `"\xA5"` | 16進数 A5（大文字も可） | 165 |
| `"\x00ff"` | 16進数 00ff（桁数制限なし） | 255 |

8進数と16進数の主な違い：
- **8進数**は最大3桁で終了（4桁目は別の文字として扱う）
- **16進数**は `\x` の後に16進数字が続く限り無制限に読み続ける

### from_hex

16進数文字を整数値に変換するヘルパー関数。`read_escaped_char` の16進数処理から利用される。

```c
int from_hex(char c) {
  if ('0' <= c && c <= '9') return c - '0';        // 0〜9
  if ('a' <= c && c <= 'f') return c - 'a' + 10;  // a〜f → 10〜15
  return c - 'A' + 10;                             // A〜F → 10〜15
}
```

### string_literal_end

閉じ `"` の位置を探す。`\` が来たら1文字読み飛ばすことでエスケープされた `\"` を誤検出しない。

```c
char *string_literal_end(char *p) {
  char *start = p;
  for (; *p != '"'; p++) {
    if (*p == '\n' || *p == '\0')
      error_at(start, "unclosed string literal");
    if (*p == '\\')
      p++; // エスケープ文字の次の1文字をスキップ
  }
  return p;
}
```

### read_string_literal

`"` が来たときに呼ばれる。`string_literal_end` で終端を求めた後、エスケープ処理しながら1バイトずつデコードして `buf` に積む。

```c
Token *read_string_literal(char *start, Token *cur) {
  char *end = string_literal_end(start + 1);
  char *buf = calloc(1, end - start); // ソース長以下のバッファで足りる
  int len = 0;

  for (char *p = start + 1; p < end;) {
    if (*p == '\\') {
      buf[len++] = read_escaped_char(&p, p + 1); // \ の次の文字を変換。p は消費分だけ進む
    } else {
      buf[len++] = *p++;
    }
  }

  Token *token = new_token(TK_STR, cur, start, end + 1 - start);
  token->ty  = array_of(ty_char, len + 1); // デコード後の長さ + ヌル終端
  token->str = buf;
  return token;
}
```

| フィールド | 値の意味 |
|---|---|
| `len`（Token） | `end + 1 - start` = ソース上の `"..."` 全体の文字数（引用符込み） |
| `ty` | `array_of(ty_char, len + 1)` = **デコード後**の文字数 + ヌル終端（`sizeof("\n") == 2`） |
| `str` | エスケープ処理済みのバイト列（ヌル終端なし。`token->ty->size - 1` バイト有効） |

---

## 主要な構造体

### Token 型

**用途**: `tokenize.c` がソース文字列を分割した結果の1トークンを表す。`parse.c` がこれを消費しながら AST を構築する。

```c
struct Token {
  TokenKind kind; // トークンの種類（TK_IDENT / TK_PUNCT / TK_KEYWORD / TK_STR / TK_NUM / TK_EOF）
  Token *next;    // 次のトークンへのポインタ（単方向連結リスト）
  int val;        // TK_NUM のときのみ有効な整数値
  char *loc;      // ソース文字列内でのトークン開始位置（元の文字列へのポインタ）
  int len;        // トークンの文字数
  Type *ty;       // TK_STR のときのみ有効。文字列リテラルの型（array_of(ty_char, len)）
  char *str;      // TK_STR のときのみ有効。文字列リテラルの内容（ヌル終端）
};
```

| フィールド | 説明 |
|---|---|
| `kind` | トークン種別。パーサが `equal()` や `consume()` で種類を判定するために使う |
| `next` | 連結リストの次ポインタ。パーサは `token = token->next` で読み進める |
| `val` | 数値リテラルの値。`TK_NUM` 以外では未使用 |
| `loc` | ソース文字列中の位置。エラーメッセージ（`error_tok`）でどこに問題があるか示すために使う |
| `len` | `loc` から何バイトがこのトークンかを示す。`equal()` での文字列比較に使う |
| `ty` | `TK_STR` のときの型情報。`array_of(ty_char, 文字数+1)` が入る（`+1` はヌル終端分） |
| `str` | `TK_STR` のときの文字列内容。引用符を除いた部分を `strndup` でコピーしたもの |

`Token` は `kind` によって使われるフィールドが変わる設計。`val`（`TK_NUM` 用）と `ty`/`str`（`TK_STR` 用）が共存している。

---

### Node 型

**用途**: パーサが構築する **抽象構文木（AST）のノード**。`gen_expr` / `gen_stmt` が再帰的にたどってアセンブリを出力する。

```c
struct Node {
  NodeKind kind;  // ノードの種類
  Node *next;     // 次のノード（文リスト・引数リスト用）
  Type *ty;       // この式の評価結果の型（add_type で設定）
  Token *tok;     // 対応するソーストークン（エラー報告用）

  Node *lhs;      // 左辺（二項演算・単項演算）
  Node *rhs;      // 右辺（二項演算）
  Node *cond;     // if / for の条件式
  Node *then;     // if / for の本体
  Node *els;      // if の else 節（なければ NULL）
  Node *init;     // for の初期化式（なければ NULL）
  Node *inc;      // for の増分式（なければ NULL）
  Node *body;     // ND_BLOCK の中身（文の連結リスト）または ND_STMT_EXPR の中身（GNU C文式）
  char *funcname; // ND_FUNCALL の関数名
  Node *args;     // ND_FUNCALL の引数リスト（next で連結）
  int val;        // ND_NUM のときのみ有効な整数値
  Obj *var;       // ND_VAR のときのみ有効。対応するローカル変数
};
```

フィールドは `NodeKind` によって使われるものが異なる（ユニオンの代わりに全フィールドを持つ設計）。

| フィールド | 主な用途 |
|---|---|
| `ty` | コード生成時にポインタ算術やロード幅の判断に使う（`add_type` が設定） |
| `tok` | エラー発生時に `error_tok(node->tok, ...)` でソース位置を報告する |
| `next` | `ND_BLOCK` の `body` 内の文リストや `ND_FUNCALL` の引数リストの連結に使う |
| `var` | `ND_VAR` ノードが対応する `Obj`（変数）を直接参照する |

---

### Type 型

**用途**: 変数・式・関数パラメータなどが持つ **型情報**。ポインタ算術のオフセット計算、配列アドレスのロードスキップ、型エラー検出などに使う。

```c
struct Type {
  TypeKind kind;   // 型の種類（TY_INT / TY_PTR / TY_FUNC / TY_ARRAY）
  int size;        // sizeof() 相当のバイト数

  Type *base;      // ポインタ・配列の「指す先/要素の型」

  Token *name;     // 宣言時の識別子トークン（変数名・関数名）

  int array_len;   // TY_ARRAY のときの要素数

  Type *return_ty; // TY_FUNC の戻り値型
  Type *params;    // TY_FUNC の引数型リスト（next で連結）
  Type *next;      // 引数リスト内の次の型
};
```

| フィールド | 説明 |
|---|---|
| `size` | 変数のスタック確保量（`new_lvar` でオフセット計算）や配列全体のバイト数に使う |
| `base` | `TY_PTR` → 指す先の型。`TY_ARRAY` → 要素型。ポインタ算術で `base->size` を掛けることで要素単位のアドレスを計算する |
| `name` | 宣言パース後に `declarator` がセットする。変数登録時に `ty->name` から変数名を取得する |
| `array_len` | `TY_ARRAY` でのみ使用。要素数を記録する（`size = base->size * array_len`） |
| `return_ty` / `params` | `TY_FUNC` でのみ使用。関数型の戻り値型・引数型を保持する |

#### 型の構造例

```
int *p        → TY_PTR { size=8, base → TY_INT { size=8 } }
int x[3]      → TY_ARRAY { size=24, array_len=3, base → TY_INT { size=8 } }
int x[2][3]   → TY_ARRAY { size=48, array_len=2,
                  base → TY_ARRAY { size=24, array_len=3,
                           base → TY_INT { size=8 } } }
int **pp      → TY_PTR { size=8, base → TY_PTR { size=8, base → TY_INT { size=8 } } }
```

`add_type` は `new_add` / `new_sub` など型に依存する演算の前に呼ぶ必要がある。

---

### Obj 型

**用途**: ローカル変数・グローバル変数・関数を統一的に表す構造体。`is_local` / `is_function` フラグで種別を区別する。

```c
struct Obj {
  Obj *next;        // 次の変数/関数（連結リスト）
  char *name;       // 変数名または関数名
  Type *ty;         // 型情報
  bool is_local;    // true = ローカル変数、false = グローバル変数/関数

  bool is_function; // true = 関数、false = 変数

  // グローバル変数用
  char *init_data;    // 初期値データへのポインタ（なければ NULL）
  char *reloc_label;  // ポインタ初期化時の再配置先ラベル（例: .L..0）
  long reloc_addend;  // 再配置時の加算オフセット（バイト単位）

  // 関数用
  Obj *params;      // 引数リスト（Obj の連結リスト）
  Node *body;       // 関数本体（compound_stmt の結果）
  Obj *locals;      // 関数内のローカル変数リスト
  int stack_size;   // 必要なスタックサイズ（16バイトアライン済み）

  // ローカル変数用
  int offset;       // rbp からのバイトオフセット（`[rbp - offset]` で参照）
};
```

| フィールド | 説明 |
|---|---|
| `is_local` | ローカル変数なら `true`。`gen_addr` がアドレス計算方法を切り替えるために使う |
| `is_function` | 関数なら `true`。`emit_data` / `emit_text` の振り分けに使う |
| `init_data` | グローバル変数の初期値データ。`NULL` なら `.zero` で出力 |
| `reloc_label` | `char *p = "foo"` のような初期化で、`.quad <label>` を出力するための参照先ラベル |
| `reloc_addend` | `char *c = msg1 + 1` のような初期化で、`.quad <label>+<addend>` を出力するためのオフセット |
| `offset` | ローカル変数の `rbp` からのオフセット。`gen_addr` で `lea rax, [rbp - offset]` として使う |

#### 文字列リテラルと init_data / reloc_label の関係

```
"abc"  →  TK_STR トークン（str="abc", ty=array_of(ty_char, 4)）
  →  new_string_literal() で匿名グローバル変数を生成
  →  文字列実体の Obj: init_data="abc\0", reloc_label=NULL
  →  emit_data() で .byte 97 / .byte 98 / .byte 99 / .byte 0 を出力

char *p = "abc";
  →  p の Obj: init_data を持ち、reloc_label に文字列ラベル名を保存
  →  emit_data() で先頭を .quad <label> として出力

char *c = msg1 + 1;
  →  c の Obj: reloc_label="msg1", reloc_addend=1（char の1要素分）
  →  emit_data() で .quad msg1+1 を出力
```

---

### VarScope / Scope 型

**用途**: ブロックスコープを管理するための構造体。`parse.c` 内で使用する。

```c
// 1つの変数エントリ（スコープ内の変数名 ↔ Obj の対応）
struct VarScope {
  VarScope *next;
  char *name;
  Obj *var;
};

// ブロックスコープ（ネストした { } に対応）
struct Scope {
  Scope *next;    // 外側のスコープへのポインタ
  VarScope *vars; // このスコープ内の変数リスト
};
```

`scope` はモジュール変数で、現在の最内スコープを指す。`enter_scope()` でスタックに積み、`leave_scope()` で1段抜ける。

```
scope →  Scope(inner) → Scope(outer) → Scope(global) → NULL
              └vars       └vars            └vars
```

`find_var()` は内側から外側に向かってこのチェーンを線形探索する。最初に一致した変数（最内スコープの変数）を返すことでシャドウイングを実現する。

---

## main.c 解説

コンパイラのエントリポイント。コマンドライン引数の解析、ファイルのトークナイズ・パース・コード生成を行う。

### CLI オプション

```
9cc [ -o <path> ] <file>
```

| オプション | 説明 |
|---|---|
| `-o <path>` / `-o<path>` | アセンブリの出力先ファイルを指定する。省略時は標準出力へ出力 |
| `--help` | 使い方を標準エラーに表示して終了（exit code 0）|

### 主な関数

| 関数 | 役割 |
|---|---|
| `parse_args` | `argc`/`argv` を走査して `opt_o`（出力先パス）と `input_path`（入力ファイル）をセットする |
| `open_file` | `opt_o` が `NULL` または `"-"` のときは `stdout` を返す。それ以外は `fopen` でファイルを開く |
| `usage` | 使い方を表示して `exit(status)` する |

### 処理フロー

```
main()
  ├─ parse_args()          ← オプション解析
  ├─ tokenize_file(input_path)  ← ファイル読み込み & トークナイズ
  ├─ parse(token)          ← AST 構築
  ├─ open_file(opt_o)      ← 出力先 FILE* を取得
  └─ codegen(prog, out)    ← アセンブリ生成 → out へ出力
```
