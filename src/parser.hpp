#pragma once
#include "ast.hpp"
#include "token.hpp"
#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>

namespace bi {

class Parser {
public:
    Parser(std::vector<Token> toks, std::string file)
        : t_(std::move(toks)), file_(std::move(file)) {}

    Program parse() {
        Program p;
        while (!check(Tok::End)) {
            if (match(Tok::Semicolon)) continue;
            p.body.push_back(declaration());
        }
        return p;
    }

private:
    std::vector<Token> t_;
    size_t             p_ = 0;
    std::string        file_;

    const Token& cur() const { return t_[p_]; }
    const Token& ahead(size_t o = 1) const {
        size_t i = p_ + o;
        return t_[i < t_.size() ? i : t_.size() - 1];
    }
    bool check(Tok k) const { return cur().type == k; }
    bool match(Tok k) { if (check(k)) { p_++; return true; } return false; }
    const Token& advance() { return t_[p_++]; }

    [[noreturn]] void err(const std::string& m) {
        throw std::runtime_error(file_ + ":" + std::to_string(cur().line) + ":" +
                                 std::to_string(cur().col) + ": " + m);
    }

    Token expect(Tok k, const char* what) {
        if (!check(k))
            err(std::string("expected ") + what + " but found '" +
                (cur().text.empty() ? "<eof>" : cur().text) + "'");
        return advance();
    }

    void semi() { match(Tok::Semicolon); }

    // ---------------- statements ----------------

    std::vector<StmtPtr> blockBody() {
        expect(Tok::LBrace, "'{'");
        std::vector<StmtPtr> out;
        while (!check(Tok::RBrace) && !check(Tok::End)) {
            if (match(Tok::Semicolon)) continue;
            out.push_back(declaration());
        }
        expect(Tok::RBrace, "'}'");
        return out;
    }

    StmtPtr declaration() {
        bool exp = false;
        if (match(Tok::Export)) exp = true;

        if (check(Tok::Let) || check(Tok::Var)) {
            int line = cur().line;
            advance();
            auto s = mkStmt(SK::Let, line);
            s->exported = exp;
            s->name = expect(Tok::Ident, "variable name").text;
            if (match(Tok::Assign)) s->expr = expression();
            semi();
            return s;
        }
        if (check(Tok::Fn) && ahead().type == Tok::Ident) return fnDecl(exp);
        if (check(Tok::Import)) return importStmt();
        if (exp) err("'export' must be followed by 'let', 'var' or 'fn'");
        return statement();
    }

    StmtPtr fnDecl(bool exp) {
        int line = cur().line;
        expect(Tok::Fn, "'fn'");
        auto s = mkStmt(SK::Func, line);
        s->exported = exp;
        s->name = expect(Tok::Ident, "function name").text;
        expect(Tok::LParen, "'('");
        if (!check(Tok::RParen)) {
            do { s->params.push_back(expect(Tok::Ident, "parameter name").text); }
            while (match(Tok::Comma));
        }
        expect(Tok::RParen, "')'");
        s->body = blockBody();
        return s;
    }

    StmtPtr importStmt() {
        int line = cur().line;
        expect(Tok::Import, "'import'");
        auto s = mkStmt(SK::Import, line);
        s->name = expect(Tok::String, "module path").text;
        if (match(Tok::As)) s->alias = expect(Tok::Ident, "alias").text;
        semi();
        return s;
    }

    StmtPtr statement() {
        if (check(Tok::LBrace)) {
            auto s = mkStmt(SK::Block, cur().line);
            s->body = blockBody();
            return s;
        }
        if (check(Tok::If))     return ifStmt();
        if (check(Tok::While))  return whileStmt();
        if (check(Tok::For))    return forStmt();
        if (check(Tok::Return)) return returnStmt();
        if (check(Tok::Route))  return routeStmt();
        if (check(Tok::Try))    return tryStmt();
        if (check(Tok::Throw))  return throwStmt();
        if (check(Tok::Break))  { int l = cur().line; advance(); semi(); return mkStmt(SK::Break, l); }
        if (check(Tok::Continue)) { int l = cur().line; advance(); semi(); return mkStmt(SK::Continue, l); }
        if (check(Tok::Fn) && ahead().type == Tok::Ident) return fnDecl(false);

        auto s = mkStmt(SK::Expr, cur().line);
        s->expr = expression();
        semi();
        return s;
    }

    StmtPtr ifStmt() {
        int line = cur().line;
        expect(Tok::If, "'if'");
        auto s = mkStmt(SK::If, line);
        expect(Tok::LParen, "'('");
        s->expr = expression();
        expect(Tok::RParen, "')'");
        s->body = blockBody();
        if (match(Tok::Else)) {
            if (check(Tok::If)) s->alt.push_back(ifStmt());
            else                s->alt = blockBody();
        }
        return s;
    }

    StmtPtr whileStmt() {
        int line = cur().line;
        expect(Tok::While, "'while'");
        auto s = mkStmt(SK::While, line);
        expect(Tok::LParen, "'('");
        s->expr = expression();
        expect(Tok::RParen, "')'");
        s->body = blockBody();
        return s;
    }

    StmtPtr forStmt() {
        int line = cur().line;
        expect(Tok::For, "'for'");
        expect(Tok::LParen, "'('");

        if ((check(Tok::Let) || check(Tok::Var)) &&
            ahead().type == Tok::Ident && ahead(2).type == Tok::In) {
            advance(); advance();
            auto s = mkStmt(SK::ForIn, line);
            s->name = t_[p_ - 1].text;
            advance();
            s->expr = expression();
            expect(Tok::RParen, "')'");
            s->body = blockBody();
            return s;
        }
        if (check(Tok::Ident) && ahead().type == Tok::In) {
            auto s = mkStmt(SK::ForIn, line);
            s->name = advance().text;
            advance();
            s->expr = expression();
            expect(Tok::RParen, "')'");
            s->body = blockBody();
            return s;
        }

        auto s = mkStmt(SK::For, line);
        if (!check(Tok::Semicolon)) s->init = forInit();
        expect(Tok::Semicolon, "';'");
        if (!check(Tok::Semicolon)) s->cond = expression();
        expect(Tok::Semicolon, "';'");
        if (!check(Tok::RParen)) s->step = expression();
        expect(Tok::RParen, "')'");
        s->body = blockBody();
        return s;
    }

    StmtPtr forInit() {
        if (check(Tok::Let) || check(Tok::Var)) {
            advance();
            auto d = mkStmt(SK::Let, cur().line);
            d->name = expect(Tok::Ident, "variable name").text;
            if (match(Tok::Assign)) d->expr = expression();
            return d;
        }
        auto e = mkStmt(SK::Expr, cur().line);
        e->expr = expression();
        return e;
    }

    StmtPtr returnStmt() {
        int line = cur().line;
        expect(Tok::Return, "'return'");
        auto s = mkStmt(SK::Return, line);
        if (!check(Tok::Semicolon) && !check(Tok::RBrace) && !check(Tok::End))
            s->expr = expression();
        semi();
        return s;
    }

    StmtPtr routeStmt() {
        int line = cur().line;
        expect(Tok::Route, "'route'");
        auto s = mkStmt(SK::Route, line);
        s->name = "GET";
        if (check(Tok::Ident) && ahead().type == Tok::String) {
            s->name = advance().text;
            for (auto& c : s->name) c = (char)std::toupper((unsigned char)c);
        }
        s->expr = expression();
        s->body = blockBody();
        return s;
    }

    StmtPtr tryStmt() {
        int line = cur().line;
        expect(Tok::Try, "'try'");
        auto s = mkStmt(SK::Try, line);
        s->body = blockBody();
        expect(Tok::Catch, "'catch'");
        expect(Tok::LParen, "'('");
        s->name = expect(Tok::Ident, "catch variable name").text;
        expect(Tok::RParen, "')'");
        s->alt = blockBody();
        return s;
    }

    StmtPtr throwStmt() {
        int line = cur().line;
        expect(Tok::Throw, "'throw'");
        auto s = mkStmt(SK::Throw, line);
        if (!check(Tok::Semicolon) && !check(Tok::RBrace) && !check(Tok::End))
            s->expr = expression();
        semi();
        return s;
    }

    // ---------------- expressions ----------------

    ExprPtr expression() { return assignment(); }

    ExprPtr assignment() {
        auto left = logicalOr();
        if (check(Tok::Assign) || check(Tok::PlusAssign) || check(Tok::MinusAssign) ||
            check(Tok::StarAssign) || check(Tok::SlashAssign)) {
            int line = cur().line;
            std::string op = advance().text;
            auto right = assignment();
            auto e = mkExpr(EK::Assign, line);
            e->op = op;
            e->a  = left;
            e->b  = right;
            return e;
        }
        return left;
    }

    ExprPtr logicalOr() {
        auto e = logicalAnd();
        while (check(Tok::Or)) {
            int line = cur().line; advance();
            auto r = mkExpr(EK::Binary, line);
            r->op = "||"; r->a = e; r->b = logicalAnd();
            e = r;
        }
        return e;
    }

    ExprPtr logicalAnd() {
        auto e = equality();
        while (check(Tok::And)) {
            int line = cur().line; advance();
            auto r = mkExpr(EK::Binary, line);
            r->op = "&&"; r->a = e; r->b = equality();
            e = r;
        }
        return e;
    }

    ExprPtr equality() {
        auto e = comparison();
        while (check(Tok::Eq) || check(Tok::Neq)) {
            int line = cur().line;
            std::string op = advance().text;
            auto r = mkExpr(EK::Binary, line);
            r->op = op; r->a = e; r->b = comparison();
            e = r;
        }
        return e;
    }

    ExprPtr comparison() {
        auto e = term();
        while (check(Tok::Lt) || check(Tok::Lte) || check(Tok::Gt) || check(Tok::Gte)) {
            int line = cur().line;
            std::string op = advance().text;
            auto r = mkExpr(EK::Binary, line);
            r->op = op; r->a = e; r->b = term();
            e = r;
        }
        return e;
    }

    ExprPtr term() {
        auto e = factor();
        while (check(Tok::Plus) || check(Tok::Minus)) {
            int line = cur().line;
            std::string op = advance().text;
            auto r = mkExpr(EK::Binary, line);
            r->op = op; r->a = e; r->b = factor();
            e = r;
        }
        return e;
    }

    ExprPtr factor() {
        auto e = unary();
        while (check(Tok::Star) || check(Tok::Slash) || check(Tok::Percent)) {
            int line = cur().line;
            std::string op = advance().text;
            auto r = mkExpr(EK::Binary, line);
            r->op = op; r->a = e; r->b = unary();
            e = r;
        }
        return e;
    }

    ExprPtr unary() {
        if (check(Tok::Not) || check(Tok::Minus)) {
            int line = cur().line;
            std::string op = advance().text;
            auto e = mkExpr(EK::Unary, line);
            e->op = op;
            e->a  = unary();
            return e;
        }
        return postfix();
    }

    ExprPtr postfix() {
        auto e = primary();
        for (;;) {
            if (match(Tok::LParen)) {
                auto c = mkExpr(EK::Call, e->line);
                c->a = e;
                if (!check(Tok::RParen)) {
                    do { c->items.push_back(expression()); } while (match(Tok::Comma));
                }
                expect(Tok::RParen, "')'");
                e = c;
            } else if (match(Tok::Dot)) {
                auto m = mkExpr(EK::Member, e->line);
                m->a   = e;
                m->str = expect(Tok::Ident, "property name").text;
                e = m;
            } else if (match(Tok::LBracket)) {
                auto i = mkExpr(EK::Index, e->line);
                i->a = e;
                i->b = expression();
                expect(Tok::RBracket, "']'");
                e = i;
            } else break;
        }
        return e;
    }

    ExprPtr primary() {
        int line = cur().line;

        if (match(Tok::Number)) { auto e = mkExpr(EK::Num, line);  e->num = t_[p_-1].num;  return e; }
        if (match(Tok::String)) { auto e = mkExpr(EK::Str, line);  e->str = t_[p_-1].text; return e; }
        if (match(Tok::True))   { auto e = mkExpr(EK::Bool, line); e->boolean = true;      return e; }
        if (match(Tok::False))  { auto e = mkExpr(EK::Bool, line); e->boolean = false;     return e; }
        if (match(Tok::Null))   return mkExpr(EK::Nil, line);
        if (match(Tok::Ident))  { auto e = mkExpr(EK::Ident, line); e->str = t_[p_-1].text; return e; }

        if (match(Tok::LParen)) {
            auto e = expression();
            expect(Tok::RParen, "')'");
            return e;
        }

        if (match(Tok::LBracket)) {
            auto e = mkExpr(EK::Array, line);
            if (!check(Tok::RBracket)) {
                do { e->items.push_back(expression()); } while (match(Tok::Comma));
            }
            expect(Tok::RBracket, "']'");
            return e;
        }

        if (match(Tok::LBrace)) {
            auto e = mkExpr(EK::Map, line);
            if (!check(Tok::RBrace)) {
                do {
                    std::string key;
                    if (check(Tok::String) || check(Tok::Ident)) key = advance().text;
                    else err("expected map key");
                    expect(Tok::Colon, "':'");
                    e->fields.emplace_back(key, expression());
                } while (match(Tok::Comma));
            }
            expect(Tok::RBrace, "'}'");
            return e;
        }

        if (match(Tok::Fn)) {
            auto e = mkExpr(EK::Func, line);
            expect(Tok::LParen, "'('");
            if (!check(Tok::RParen)) {
                do { e->params.push_back(expect(Tok::Ident, "parameter name").text); }
                while (match(Tok::Comma));
            }
            expect(Tok::RParen, "')'");
            e->body = blockBody();
            return e;
        }

        err("unexpected token '" + (cur().text.empty() ? "<eof>" : cur().text) + "'");
    }
};

} // namespace bi