/*
 * parser
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "9cc.h"

// トークンの種類
typedef enum {
    TK_RESERVED,  // 記号
    TK_IDENT,     // 識別子
    TK_NUM,       // 整数
    TK_RETURN,    // return
    TK_EOF,       // 入力の終わり
} TokenKind;

typedef struct Token Token;

// トークン型
struct Token {
    TokenKind kind;   // トークンの型
    Token *next;      // 次の入力トークン
    int val;          // TK_NUMの場合、その数値
    const char *str;  // トークン文字列
    int len;          // トークンの長さ
};

// ローカル変数
LVar *locals;

// Function                     // EBNF
static void program(void);      // program    = stmt*
static Node *stmt(void);        // stmt       = expr ";" | "return" expr ";"
static Node *expr(void);        // expr       = assign
static Node *assign(void);      // assign     = equality ("=" assign)?
static Node *equality(void);    // equality   = relational ("==" relational | "!=" relational)*
static Node *relational(void);  // relational = add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *add(void);         // add        = mul ("+" mul | "-" mul)*
static Node *mul(void);         // mul        = unary ("*" unary | "/" unary)*
static Node *unary(void);       // unary      = ("+" | "-")? primary
static Node *primary(void);     // primary    = num | ident | "(" expr ")"

// 入力プログラム
const char *user_input;
// 現在着目しているトークン
static Token *token;
// 文単位のAST
static Node *code[100];


// エラー箇所を報告する
static void error_at(const char *loc, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    int pos = loc - user_input;
    fprintf(stderr, "%s\n", user_input);
    fprintf(stderr, "%*s", pos, "");  // pos個の空白を出力
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// 次のトークンが期待している記号の場合、トークンを1つ進めて真を返す
// それ以外の場合、偽を返す
static bool consume(char *op) {
    if (token->kind != TK_RESERVED ||
            strlen(op) != token->len ||
            memcmp(token->str, op, token->len))
        return false;
    token = token->next;
    return true;
}

// 次のトークンが kind の場合、トークンを1つ進めてそのトークンを返す
// それ以外の場合、NULLを返す
static Token *consume_token(TokenKind kind) {
    if (token->kind != kind)
        return NULL;
    Token *tmp = token;
    token = token->next;
    return tmp;
}

// 次のトークンが期待している記号の場合、トークンを1つ進める
// それ以外の場合、エラーを報告して終了
static void expect(char *op) {
    if (token->kind != TK_RESERVED ||
            strlen(op) != token->len ||
            memcmp(token->str, op, token->len))
        error_at(token->str, "'%s'ではありません", op);
    token = token->next;
}

// 次のトークンが数値の場合、トークンを1つ進めてその数値を返す
// それ以外の場合、エラーを報告して終了
static int expect_number() {
    if (token->kind != TK_NUM)
        error_at(token->str, "数値ではありません");
    int val = token->val;
    token = token->next;
    return val;
}

static bool at_eof() {
    return token->kind == TK_EOF;
}

// 新しいトークンを作成してcurに繋げる
static Token *new_token(TokenKind kind, Token *cur, const char *str, int len) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
    cur->next = tok;
    return tok;
}

static bool is_alnum(char c) {
    return ('a' <= c && c <= 'z') ||
        ('A' <= c && c <= 'Z') ||
        ('0' <= c && c <= '9') ||
        (c == '_');
}

static bool equal_str(const char *s, const char *t) {
    return strncmp(s, t, strlen(t)) == 0;
}

// 入力文字列pからトークンを取り出してそれを返す
static Token *tokenize(const char *p) {
    Token head;
    head.next = NULL;
    Token *cur = &head;

    while (*p) {
        // 空白文字をスキップ
        if (isspace(*p)) {
            p++;
            continue;
        }

        // 複数文字トークン
        if (equal_str(p, "<=") ||
                equal_str(p, ">=") ||
                equal_str(p, "==") ||
                equal_str(p, "!=")) {
            cur = new_token(TK_RESERVED, cur, p, 2);
            p += 2;
            continue;
        }

        // 一文字トークン
        switch (*p) {
        case '+':
        case '-':
        case '*':
        case '/':
        case '(':
        case ')':
        case '<':
        case '>':
        case '=':
        case ';':
            cur = new_token(TK_RESERVED, cur, p++, 1);
            continue;
        }

        // 数字
        if (isdigit(*p)) {
            char *newp;
            int val = strtol(p, &newp, 10);
            cur = new_token(TK_NUM, cur, p, newp - p);
            cur->val = val;
            p = newp;
            continue;
        }

        // 予約語
        if (strncmp(p, "return", 6) == 0 && !is_alnum(p[6])) {
            cur = new_token(TK_RETURN, cur, p, 6);
            p += 6;
            continue;
        }

        // 識別子 [a-z]+
        const char *ident = p;
        while ('a' <= *p && *p <= 'z')
            p++;
        if (ident != p) {
            cur = new_token(TK_IDENT, cur, ident, p - ident);
            continue;
        }

        error_at(p, "トークナイズできません");
    }

    new_token(TK_EOF, cur, p, 0);
    return head.next;
}

static Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

static Node *new_node_num(int val) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->val = val;
    return node;
}

static void program() {
    int i = 0;
    while (!at_eof())
        code[i++] = stmt();
    code[i] = NULL;

}

static Node *stmt() {
    Node *node;

    if (consume_token(TK_RETURN)) {
        node = new_node(ND_RETURN, expr(), NULL);
    } else {
        node = expr();
    }

    expect(";");
    return node;
}

static Node *expr() {
    return assign();
}

static Node *assign() {
    Node *node = equality();
    if (consume("="))
        node = new_node(ND_ASSIGN, node, assign());
    return node;
}

static Node *equality() {
    Node *node = relational();

    for (;;) {
        if (consume("=="))
            node = new_node(ND_EQ, node, relational());
        else if (consume("!="))
            node = new_node(ND_NE, node, relational());
        else
            return node;
    }
}

static Node *relational() {
    Node *node = add();

    for (;;) {
        if (consume("<"))
            node = new_node(ND_LT, node, add());
        else if (consume("<="))
            node = new_node(ND_LE, node, add());
        else if (consume(">"))
            node = new_node(ND_GT, node, add());
        else if (consume(">="))
            node = new_node(ND_GE, node, add());
        else
            return node;
    }
}

static Node *add() {
    Node *node = mul();

    for (;;) {
        if (consume("+"))
            node = new_node(ND_ADD, node, mul());
        else if (consume("-"))
            node = new_node(ND_SUB, node, mul());
        else
            return node;
    }
}

static Node *mul() {
    Node *node = unary();

    for (;;) {
        if (consume("*"))
            node = new_node(ND_MUL, node, unary());
        else if (consume("/"))
            node = new_node(ND_DIV, node, unary());
        else
            return node;
    }
}

static Node *unary() {
    if (consume("+"))
        return primary();
    if (consume("-"))
        return new_node(ND_SUB, new_node_num(0), primary());  // 0-x
    return primary();
}

static LVar *find_lvar(Token *tok) {
    for (LVar *var = locals; var; var = var->next)
        if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
            return var;
    return NULL;
}

static Node *primary() {
    // 次のトークンが"("なら、 "(" expr ")" のはず
    if (consume("(")) {
        Node *node = expr();
        expect(")");
        return node;
    }

    Token *tok = consume_token(TK_IDENT);
    if (tok) {
        // 変数
        Node *node = calloc(1, sizeof(Node));
        node->kind = ND_LVAR;

        LVar *lvar = find_lvar(tok);
        if (lvar) {
            // 作成済み
            node->offset = lvar->offset;
        } else {
            // 新規
            lvar = calloc(1, sizeof(LVar));
            lvar->next = locals;
            lvar->name = tok->str;
            lvar->len = tok->len;
            if (locals != NULL)
                lvar->offset = locals->offset + 8;
            node->offset = lvar->offset;
            locals = lvar;
        }
        return node;
    }

    // そうでなければ数値のはず
    return new_node_num(expect_number());
}

Node **parse(const char *str) {
    user_input = str;

    // トークナイズしてパースする
    token = tokenize(str);
    program();
    return code;
}
