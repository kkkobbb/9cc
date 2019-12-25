/*
 * 9cc header
 */

// 抽象構文木のノードの種類
typedef enum {
    ND_ADD,     // +
    ND_SUB,     // -
    ND_MUL,     // *
    ND_DIV,     // /
    ND_EQ,      // ==
    ND_NE,      // !=
    ND_LT,      // <
    ND_LE,      // <=
    ND_GT,      // >
    ND_GE,      // >=
    ND_ASSIGN,  // =
    ND_LVAR,    // ローカル変数
    ND_NUM,     // 整数
    ND_RETURN,  // return
} NodeKind;

typedef struct Node Node;

// 抽象構文木のノードの型
struct Node {
    NodeKind kind;  // ノードの型
    Node *lhs;      // 左辺
    Node *rhs;      // 右辺
    int val;        // kindがND_NUMの場合のみ使う
    int offset;     // kindがND_LVARの場合のみ使う
};

typedef struct LVar LVar;

// ローカル変数型
struct LVar {
    LVar *next;  // 次の変数がNULL
    const char *name;  // 変数の名前
    int len;     // 名前の長さ
    int offset;  // RBPからのオフセット
};

// ローカル変数
extern LVar *locals;


Node **parse(const char *str);
void codegen(Node *node[]);
