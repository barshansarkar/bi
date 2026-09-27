#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace bi {

struct Expr;
struct Stmt;
using ExprPtr = std::shared_ptr<Expr>;
using StmtPtr = std::shared_ptr<Stmt>;

enum class EK {
    Num, Str, Bool, Nil, Ident,
    Array, Map, Unary, Binary, Call,
    Member, Index, Assign, Func
};

struct Expr {
    EK          kind;
    int         line = 0;
    double      num  = 0;
    std::string str;
    bool        boolean = false;
    std::string op;

    ExprPtr a, b, c;
    std::vector<ExprPtr> items;
    std::vector<std::pair<std::string, ExprPtr>> fields;
    std::vector<std::string> params;
    std::vector<StmtPtr>     body;
};

inline ExprPtr mkExpr(EK k, int line) {
    auto e = std::make_shared<Expr>();
    e->kind = k;
    e->line = line;
    return e;
}

enum class SK {
    Let, Expr, Block, If, While, For, ForIn,
    Return, Break, Continue, Func, Route, Import,
    Try, Throw
};

struct Stmt {
    SK          kind;
    int         line = 0;
    std::string name;
    std::string alias;
    std::vector<std::string> params;
    bool        exported = false;

    ExprPtr expr;                 // let init / if cond / while cond / return val / route path / throw value
    StmtPtr init;                 // for-init
    ExprPtr cond, step;           // for-cond / for-step
    std::vector<StmtPtr> body;    // block / try-body
    std::vector<StmtPtr> alt;     // else branch / catch-body
};

inline StmtPtr mkStmt(SK k, int line) {
    auto s = std::make_shared<Stmt>();
    s->kind = k;
    s->line = line;
    return s;
}

struct Program { std::vector<StmtPtr> body; };

} // namespace bi