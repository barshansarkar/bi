#pragma once
#include "ast.hpp"
#include "builtins.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "value.hpp"

#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

namespace bi {

struct ReturnSignal   { Value value; };
struct BreakSignal    {};
struct ContinueSignal {};

class Interpreter {
public:
    Interpreter() {
        global_ = std::make_shared<Env>();
        g_callFn = [this](const Value& f, ValueList& a) { return call(f, a); };
        registerBuiltins(global_);
    }

    std::shared_ptr<Env> globals() const { return global_; }

    const std::vector<std::shared_ptr<Function>>& routes() const { return routes_; }
    std::string baseDir() const { return baseDir_; }

    // ---------- loading ----------

    void runFile(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) throw std::runtime_error("cannot open file: " + path);
        std::stringstream ss; ss << f.rdbuf();

        auto slash = path.find_last_of("/\\");
        baseDir_ = (slash == std::string::npos) ? "." : path.substr(0, slash);

        runSource(ss.str(), path);
    }

    void runSource(const std::string& src, const std::string& file) {
        Lexer  lx(src, file);
        Parser ps(lx.run(), file);
        Program prog = ps.parse();
        for (auto& s : prog.body) exec(s, global_);
    }

    // ---------- evaluation ----------

    Value eval(ExprPtr e, std::shared_ptr<Env> env) {
        switch (e->kind) {
            case EK::Num:  return vnum(e->num);
            case EK::Str:  return vstr(e->str);
            case EK::Bool: return vbool(e->boolean);
            case EK::Nil:  return vnil();

            case EK::Ident: {
                Value* p = env->find(e->str);
                if (!p) throw std::runtime_error("undefined variable '" + e->str + "'");
                return *p;
            }

            case EK::Array: {
                auto list = std::make_shared<ValueList>();
                for (auto& it : e->items) list->push_back(eval(it, env));
                return varr(list);
            }

            case EK::Map: {
                auto m = std::make_shared<ValueMap>();
                for (auto& kv : e->fields) (*m)[kv.first] = eval(kv.second, env);
                return vmap(m);
            }

            case EK::Unary: {
                Value a = eval(e->a, env);
                if (e->op == "!") return vbool(!truthy(a));
                if (e->op == "-") return vnum(-toNum(a));
                throw std::runtime_error("bad unary operator " + e->op);
            }

            case EK::Binary:  return evalBinary(e, env);
            case EK::Assign:  return evalAssign(e, env);
            case EK::Member:  return getMember(eval(e->a, env), e->str);

            case EK::Index: {
                Value o = eval(e->a, env);
                Value k = eval(e->b, env);
                return index(o, k);
            }

            case EK::Call: {
                // method call:  obj.method(args)
                if (e->a->kind == EK::Member) {
                    Value obj = eval(e->a->a, env);
                    Value m   = getMethod(obj, e->a->str);
                    if (m.type == Value::NATIVE) {
                        ValueList args;
                        args.push_back(obj);
                        for (auto& a : e->items) args.push_back(eval(a, env));
                        return m.native(args);
                    }
                }
                Value callee = eval(e->a, env);
                ValueList args;
                for (auto& a : e->items) args.push_back(eval(a, env));
                return call(callee, args);
            }

            case EK::Func: {
                auto f = std::make_shared<Function>();
                f->name    = "lambda";
                f->params  = e->params;
                f->body    = e->body;
                f->closure = env;
                Value v; v.type = Value::FUNC; v.func = f;
                return v;
            }
        }
        return vnil();
    }

    // ---------- statements ----------

    void exec(StmtPtr s, std::shared_ptr<Env> env) {
        switch (s->kind) {

            case SK::Let: {
                Value v = s->expr ? eval(s->expr, env) : vnil();
                env->define(s->name, v);
                if (s->exported && exportSink_) (*exportSink_)[s->name] = v;
                break;
            }

            case SK::Expr:
                eval(s->expr, env);
                break;

            case SK::Block:
                execBlock(s->body, env);
                break;

            case SK::If: {
                if (truthy(eval(s->expr, env)))    execBlock(s->body, env);
                else if (!s->alt.empty())          execBlock(s->alt, env);
                break;
            }

            case SK::While: {
                while (truthy(eval(s->expr, env))) {
                    try { execBlock(s->body, env); }
                    catch (BreakSignal&)    { break; }
                    catch (ContinueSignal&) { continue; }
                }
                break;
            }

            case SK::For: {
                auto scope = env->child();
                if (s->init) exec(s->init, scope);
                for (;;) {
                    if (s->cond && !truthy(eval(s->cond, scope))) break;
                    bool brk = false;
                    try { execBlock(s->body, scope); }
                    catch (BreakSignal&)    { brk = true; }
                    catch (ContinueSignal&) {}
                    if (brk) break;
                    if (s->step) eval(s->step, scope);
                }
                break;
            }

            case SK::ForIn: {
                Value it = eval(s->expr, env);
                auto scope = env->child();
                auto iter = [&](const Value& v) {
                    scope->define(s->name, v);
                    try { execBlock(s->body, scope); return true; }
                    catch (BreakSignal&)    { return false; }
                    catch (ContinueSignal&) { return true; }
                };
                if (it.type == Value::ARR) {
                    for (auto& x : *it.arr) if (!iter(x)) break;
                } else if (it.type == Value::MAP) {
                    for (auto& kv : *it.map) if (!iter(vstr(kv.first))) break;
                } else if (it.type == Value::STR) {
                    for (char c : it.str) if (!iter(vstr(std::string(1, c)))) break;
                }
                break;
            }

            case SK::Return:
                throw ReturnSignal{ s->expr ? eval(s->expr, env) : vnil() };

            case SK::Break:    throw BreakSignal{};
            case SK::Continue: throw ContinueSignal{};

            case SK::Func: {
                auto f = std::make_shared<Function>();
                f->name    = s->name;
                f->params  = s->params;
                f->body    = s->body;
                f->closure = env;
                Value v; v.type = Value::FUNC; v.func = f;
                env->define(s->name, v);
                if (s->exported && exportSink_) (*exportSink_)[s->name] = v;
                break;
            }

            case SK::Route: {
                auto f = std::make_shared<Function>();
                f->name        = "route " + s->name;
                f->body        = s->body;
                f->closure     = env;
                f->isRoute     = true;
                f->routeMethod = s->name;
                Value pv       = eval(s->expr, env);
                f->routePath   = (pv.type == Value::STR) ? pv.str : toStr(pv);
                routes_.push_back(f);
                break;
            }

            case SK::Import: {
                Value mod = importModule(s->name);
                std::string bind = s->alias.empty() ? moduleBase(s->name) : s->alias;
                env->define(bind, mod);
                break;
            }
        }
    }

    void execBlock(const std::vector<StmtPtr>& body, std::shared_ptr<Env> env) {
        auto scope = env->child();
        for (auto& s : body) exec(s, scope);
    }

    // ---------- calls ----------

    Value call(const Value& fn, ValueList& args) {
        if (fn.type == Value::NATIVE) return fn.native(args);
        if (fn.type == Value::FUNC)   return callFunction(fn.func, args);
        throw std::runtime_error("attempt to call a " + typeName(fn));
    }

    Value callFunction(std::shared_ptr<Function> f, ValueList& args) {
        auto env = f->closure->child();
        for (size_t i = 0; i < f->params.size(); i++)
            env->define(f->params[i], i < args.size() ? args[i] : vnil());

        auto rest = std::make_shared<ValueList>();
        for (size_t i = f->params.size(); i < args.size(); i++) rest->push_back(args[i]);
        env->define("args", varr(rest));

        try {
            for (auto& s : f->body) exec(s, env);
        } catch (ReturnSignal& r) {
            return r.value;
        }
        return vnil();
    }

    // ---------- internals ----------

    std::map<std::string, Value>* exportSink_ = nullptr;

private:
    std::shared_ptr<Env> global_;
    std::string          baseDir_ = ".";
    std::vector<std::shared_ptr<Function>> routes_;
    std::map<std::string, Value> moduleCache_;

    Value evalBinary(ExprPtr e, std::shared_ptr<Env> env) {
        if (e->op == "&&") {
            Value a = eval(e->a, env);
            if (!truthy(a)) return vbool(false);
            return vbool(truthy(eval(e->b, env)));
        }
        if (e->op == "||") {
            Value a = eval(e->a, env);
            if (truthy(a)) return vbool(true);
            return vbool(truthy(eval(e->b, env)));
        }

        Value a = eval(e->a, env);
        Value b = eval(e->b, env);

        if (e->op == "==") return vbool(valueEquals(a, b));
        if (e->op == "!=") return vbool(!valueEquals(a, b));

        if (e->op == "+") {
            if (a.type == Value::STR || b.type == Value::STR) return vstr(toStr(a) + toStr(b));
            if (a.type == Value::ARR && b.type == Value::ARR) {
                auto out = std::make_shared<ValueList>(*a.arr);
                for (auto& x : *b.arr) out->push_back(x);
                return varr(out);
            }
            return vnum(toNum(a) + toNum(b));
        }
        if (e->op == "-") return vnum(toNum(a) - toNum(b));
        if (e->op == "*") {
            if (a.type == Value::STR && b.type == Value::NUM) {
                std::string out;
                for (int i = 0; i < (int)b.num; i++) out += a.str;
                return vstr(out);
            }
            return vnum(toNum(a) * toNum(b));
        }
        if (e->op == "/") {
            double d = toNum(b);
            if (d == 0) throw std::runtime_error("division by zero");
            return vnum(toNum(a) / d);
        }
        if (e->op == "%") {
            double d = toNum(b);
            if (d == 0) throw std::runtime_error("modulo by zero");
            return vnum(std::fmod(toNum(a), d));
        }
        if (e->op == "<")  return vbool(toNum(a) <  toNum(b));
        if (e->op == "<=") return vbool(toNum(a) <= toNum(b));
        if (e->op == ">")  return vbool(toNum(a) >  toNum(b));
        if (e->op == ">=") return vbool(toNum(a) >= toNum(b));

        throw std::runtime_error("unknown binary operator " + e->op);
    }

    Value evalAssign(ExprPtr e, std::shared_ptr<Env> env) {
        Value rhs;
        if (e->op == "=") {
            rhs = eval(e->b, env);
        } else {
            Value cur = eval(e->a, env);
            Value b   = eval(e->b, env);
            if (e->op == "+=") {
                rhs = (cur.type == Value::STR || b.type == Value::STR)
                        ? vstr(toStr(cur) + toStr(b))
                        : vnum(toNum(cur) + toNum(b));
            } else if (e->op == "-=") rhs = vnum(toNum(cur) - toNum(b));
            else if (e->op == "*=")   rhs = vnum(toNum(cur) * toNum(b));
            else if (e->op == "/=") {
                if (toNum(b) == 0) throw std::runtime_error("division by zero");
                rhs = vnum(toNum(cur) / toNum(b));
            }
        }

        if (e->a->kind == EK::Ident) {
            env->assign(e->a->str, rhs);
            return rhs;
        }
        if (e->a->kind == EK::Member) {
            Value obj = eval(e->a->a, env);
            if (obj.type == Value::MAP) { (*obj.map)[e->a->str] = rhs; return rhs; }
            throw std::runtime_error("cannot assign property on " + typeName(obj));
        }
        if (e->a->kind == EK::Index) {
            Value obj = eval(e->a->a, env);
            Value k   = eval(e->a->b, env);
            if (obj.type == Value::ARR) {
                int i = (int)toNum(k);
                if (i < 0 || i >= (int)obj.arr->size())
                    throw std::runtime_error("array index out of range: " + std::to_string(i));
                (*obj.arr)[i] = rhs;
                return rhs;
            }
            if (obj.type == Value::MAP) { (*obj.map)[toStr(k)] = rhs; return rhs; }
            throw std::runtime_error("cannot index-assign on " + typeName(obj));
        }
        throw std::runtime_error("invalid assignment target");
    }

    Value index(const Value& o, const Value& k) {
        if (o.type == Value::ARR) {
            int i = (int)toNum(k);
            if (i < 0) i += (int)o.arr->size();
            if (i < 0 || i >= (int)o.arr->size()) return vnil();
            return (*o.arr)[i];
        }
        if (o.type == Value::MAP) {
            auto it = o.map->find(toStr(k));
            return it != o.map->end() ? it->second : vnil();
        }
        if (o.type == Value::STR) {
            int i = (int)toNum(k);
            if (i < 0 || i >= (int)o.str.size()) return vnil();
            return vstr(std::string(1, o.str[i]));
        }
        return vnil();
    }

    // ---------- modules ----------

    static std::string moduleBase(const std::string& path) {
        std::string p = path;
        auto slash = p.find_last_of('/');
        if (slash != std::string::npos) p = p.substr(slash + 1);
        if (p.size() > 3 && p.substr(p.size() - 3) == ".bi") p = p.substr(0, p.size() - 3);
        return p;
    }

    std::string resolveModule(const std::string& name) {
        // relative paths
        if (name.rfind("./", 0) == 0 || name.rfind("../", 0) == 0 || name[0] == '/') {
            std::string p = name;
            if (p.size() < 3 || p.substr(p.size() - 3) != ".bi") p += ".bi";
            return p;
        }
        // installed packages
        const std::string candidates[] = {
            "bi_modules/" + name + "/src/main.bi",
            "bi_modules/" + name + "/index.bi",
            "bi_modules/" + name + "/main.bi",
            "bi_modules/" + name + "/" + name + ".bi",
            name + ".bi",
        };
        for (auto& c : candidates) {
            std::ifstream f(c);
            if (f.good()) return c;
        }
        throw std::runtime_error("cannot resolve module '" + name + "'");
    }

    Value importModule(const std::string& name) {
        std::string path = resolveModule(name);

        auto cached = moduleCache_.find(path);
        if (cached != moduleCache_.end()) return cached->second;

        std::ifstream f(path, std::ios::binary);
        if (!f) throw std::runtime_error("cannot open module: " + path);
        std::stringstream ss; ss << f.rdbuf();

        Lexer  lx(ss.str(), path);
        Parser ps(lx.run(), path);
        Program prog = ps.parse();

        auto mEnv = global_->child();
        auto exports = std::make_shared<ValueMap>();

        auto* prevSink = exportSink_;
        exportSink_ = exports.get();
        try {
            for (auto& s : prog.body) exec(s, mEnv);
        } catch (...) {
            exportSink_ = prevSink;
            throw;
        }
        exportSink_ = prevSink;

        Value mod = vmap(exports);
        moduleCache_[path] = mod;
        return mod;
    }
};

} // namespace bi