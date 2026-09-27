#pragma once
#include "ast.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace bi {

struct Value;
struct Function;
struct Env;

// ============================================================
//  Insertion-order map (like JS Map / Python dict).
//  API-compatible subset of std::map used by the interpreter.
// ============================================================
template <typename V>
class OrderedMapT {
public:
    using Pair      = std::pair<std::string, V>;
    using Container = std::vector<Pair>;
    using iterator       = typename Container::iterator;
    using const_iterator = typename Container::const_iterator;

    iterator       begin()       { return items_.begin(); }
    iterator       end()         { return items_.end(); }
    const_iterator begin() const { return items_.begin(); }
    const_iterator end()   const { return items_.end(); }

    iterator find(const std::string& k) {
        for (auto it = items_.begin(); it != items_.end(); ++it)
            if (it->first == k) return it;
        return items_.end();
    }
    const_iterator find(const std::string& k) const {
        for (auto it = items_.begin(); it != items_.end(); ++it)
            if (it->first == k) return it;
        return items_.end();
    }

    size_t count(const std::string& k) const {
        for (auto& p : items_) if (p.first == k) return 1;
        return 0;
    }
    size_t size()  const { return items_.size(); }
    bool   empty() const { return items_.empty(); }

    V& operator[](const std::string& k) {
        for (auto& p : items_) if (p.first == k) return p.second;
        items_.emplace_back(k, V{});
        return items_.back().second;
    }

    size_t erase(const std::string& k) {
        for (auto it = items_.begin(); it != items_.end(); ++it)
            if (it->first == k) { items_.erase(it); return 1; }
        return 0;
    }

private:
    Container items_;
};

using ValueList = std::vector<Value>;
using ValueMap  = OrderedMapT<Value>;
using NativeFn  = std::function<Value(ValueList&)>;

// ============================================================
//  Non-error unwind marker (so `catch(std::exception&)` won't
//  accidentally swallow return/break/continue/serve).
// ============================================================
struct ControlSignal {};

struct ServeSignal : ControlSignal {
    int port;
    explicit ServeSignal(int p) : port(p) {}
};

// ============================================================
//  Runtime error with optional source location.
// ============================================================
struct BiError : std::runtime_error {
    int line = 0;
    BiError(const std::string& msg, int ln = 0)
        : std::runtime_error(msg), line(ln) {}
};

// ============================================================
//  HTTP response context (per-thread).
// ============================================================
struct Response {
    int status = 200;
    std::string contentType = "text/html; charset=utf-8";
    std::string body;
    std::map<std::string, std::string> headers;
};

inline thread_local Response* currentResponse = nullptr;

// ============================================================
//  Value
// ============================================================
struct Value {
    enum Type { NIL, NUM, BOOL, STR, ARR, MAP, FUNC, NATIVE };

    Type        type = NIL;
    double      num  = 0;
    bool        boolean = false;
    std::string str;

    std::shared_ptr<ValueList> arr;
    std::shared_ptr<ValueMap>  map;
    std::shared_ptr<Function>  func;
    NativeFn                   native;
};

inline Value vnil()  { Value v; v.type = Value::NIL;  return v; }
inline Value vnum(double d)   { Value v; v.type = Value::NUM;  v.num = d; return v; }
inline Value vbool(bool b)    { Value v; v.type = Value::BOOL; v.boolean = b; return v; }
inline Value vstr(const std::string& s) { Value v; v.type = Value::STR; v.str = s; return v; }
inline Value varr(std::shared_ptr<ValueList> a) { Value v; v.type = Value::ARR; v.arr = std::move(a); return v; }
inline Value vmap(std::shared_ptr<ValueMap> m)  { Value v; v.type = Value::MAP; v.map = std::move(m); return v; }

struct Function {
    std::string              name;
    std::vector<std::string> params;
    std::vector<StmtPtr>     body;
    std::shared_ptr<Env>     closure;

    bool        isRoute = false;
    std::string routeMethod = "GET";
    std::string routePath   = "/";
};

struct Env : std::enable_shared_from_this<Env> {
    std::shared_ptr<Env> parent;
    ValueMap             vars;

    std::shared_ptr<Env> child() {
        auto e = std::make_shared<Env>();
        e->parent = shared_from_this();
        return e;
    }

    Value* find(const std::string& n) {
        auto it = vars.find(n);
        if (it != vars.end()) return &it->second;
        return parent ? parent->find(n) : nullptr;
    }

    void define(const std::string& n, Value v) { vars[n] = std::move(v); }

    void assign(const std::string& n, Value v) {
        Value* p = find(n);
        if (p) *p = std::move(v);
        else   vars[n] = std::move(v);
    }
};

// ============================================================
//  Helpers
// ============================================================
inline std::string typeName(const Value& v) {
    switch (v.type) {
        case Value::NIL:    return "null";
        case Value::NUM:    return "number";
        case Value::BOOL:   return "bool";
        case Value::STR:    return "string";
        case Value::ARR:    return "array";
        case Value::MAP:    return "map";
        case Value::FUNC:
        case Value::NATIVE: return "function";
    }
    return "?";
}

inline bool truthy(const Value& v) {
    switch (v.type) {
        case Value::NIL:  return false;
        case Value::BOOL: return v.boolean;
        case Value::NUM:  return v.num != 0;
        case Value::STR:  return !v.str.empty();
        default:          return true;
    }
}

inline double toNum(const Value& v) {
    switch (v.type) {
        case Value::NUM:  return v.num;
        case Value::BOOL: return v.boolean ? 1 : 0;
        case Value::NIL:  return 0;
        case Value::STR:
            try { return std::stod(v.str); } catch (...) { return 0; }
        default: return 0;
    }
}

inline std::string toStr(const Value& v) {
    switch (v.type) {
        case Value::NIL:  return "null";
        case Value::BOOL: return v.boolean ? "true" : "false";
        case Value::NUM: {
            double d = v.num;
            // Guard the cast so we never hit UB on huge doubles.
            if (std::fabs(d) < 9.0e15 && d == (double)(long long)d)
                return std::to_string((long long)d);
            std::ostringstream os; os << d; return os.str();
        }
        case Value::STR: return v.str;
        case Value::ARR: {
            std::string out = "[";
            for (size_t i = 0; i < v.arr->size(); i++) {
                if (i) out += ", ";
                const Value& x = (*v.arr)[i];
                out += (x.type == Value::STR) ? ("\"" + x.str + "\"") : toStr(x);
            }
            return out + "]";
        }
        case Value::MAP: {
            std::string out = "{";
            bool first = true;
            for (auto& kv : *v.map) {
                if (!first) out += ", ";
                first = false;
                out += kv.first + ": " + toStr(kv.second);
            }
            return out + "}";
        }
        case Value::FUNC:   return "<fn " + (v.func ? v.func->name : std::string("?")) + ">";
        case Value::NATIVE: return "<native fn>";
    }
    return "";
}

inline bool valueEquals(const Value& a, const Value& b) {
    if (a.type != b.type) {
        if ((a.type == Value::NUM && b.type == Value::BOOL) ||
            (a.type == Value::BOOL && b.type == Value::NUM))
            return toNum(a) == toNum(b);
        return false;
    }
    switch (a.type) {
        case Value::NIL:  return true;
        case Value::NUM:  return a.num == b.num;
        case Value::BOOL: return a.boolean == b.boolean;
        case Value::STR:  return a.str == b.str;
        case Value::ARR:
            if (a.arr->size() != b.arr->size()) return false;
            for (size_t i = 0; i < a.arr->size(); i++)
                if (!valueEquals((*a.arr)[i], (*b.arr)[i])) return false;
            return true;
        case Value::MAP:
            if (a.map->size() != b.map->size()) return false;
            for (auto& kv : *a.map) {
                auto it = b.map->find(kv.first);
                if (it == b.map->end() || !valueEquals(kv.second, it->second)) return false;
            }
            return true;
        default: return a.func == b.func;
    }
}

// ============================================================
//  JSON
// ============================================================
inline std::string toJson(const Value& v) {
    switch (v.type) {
        case Value::NIL:  return "null";
        case Value::BOOL: return v.boolean ? "true" : "false";
        case Value::NUM: {
            double d = v.num;
            if (std::fabs(d) < 9.0e15 && d == (double)(long long)d)
                return std::to_string((long long)d);
            std::ostringstream os; os << d; return os.str();
        }
        case Value::STR: {
            std::string out = "\"";
            for (unsigned char c : v.str) {
                switch (c) {
                    case '"':  out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\n': out += "\\n";  break;
                    case '\r': out += "\\r";  break;
                    case '\t': out += "\\t";  break;
                    default:
                        if (c < 0x20) {
                            char buf[8];
                            std::snprintf(buf, sizeof buf, "\\u%04x", c);
                            out += buf;
                        } else out += (char)c;
                }
            }
            return out + "\"";
        }
        case Value::ARR: {
            std::string out = "[";
            for (size_t i = 0; i < v.arr->size(); i++) {
                if (i) out += ",";
                out += toJson((*v.arr)[i]);
            }
            return out + "]";
        }
        case Value::MAP: {
            std::string out = "{";
            bool first = true;
            for (auto& kv : *v.map) {
                if (!first) out += ",";
                first = false;
                out += "\"" + kv.first + "\":" + toJson(kv.second);
            }
            return out + "}";
        }
        default: return "null";
    }
}

struct JsonParser {
    const std::string& s;
    size_t i = 0;
    explicit JsonParser(const std::string& src) : s(src) {}

    void ws() { while (i < s.size() && (s[i]==' '||s[i]=='\t'||s[i]=='\n'||s[i]=='\r')) i++; }
    [[noreturn]] void err(const char* m) { throw std::runtime_error(std::string("JSON: ") + m); }

    void expect(const char* lit) {
        size_t n = std::strlen(lit);
        if (s.compare(i, n, lit) != 0) err("bad literal");
        i += n;
    }

    Value parse() { ws(); Value v = value(); ws(); return v; }

    Value value() {
        ws();
        if (i >= s.size()) err("unexpected end");
        char c = s[i];
        if (c == '{') return object();
        if (c == '[') return array();
        if (c == '"') return vstr(strv());
        if (c == 't') { expect("true");  return vbool(true); }
        if (c == 'f') { expect("false"); return vbool(false); }
        if (c == 'n') { expect("null");  return vnil(); }
        return number();
    }

    std::string strv() {
        if (s[i] != '"') err("expected string");
        i++;
        std::string out;
        while (i < s.size() && s[i] != '"') {
            char c = s[i++];
            if (c == '\\' && i < s.size()) {
                char e = s[i++];
                switch (e) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case '/': out += '/';  break;
                    case '"': out += '"';  break;
                    case '\\': out += '\\'; break;
                    case 'u': {
                        if (i + 4 > s.size()) err("bad \\u");
                        int cp = (int)std::strtol(s.substr(i, 4).c_str(), nullptr, 16);
                        i += 4;
                        if (cp < 0x80) out += (char)cp;
                        else if (cp < 0x800) {
                            out += (char)(0xC0 | (cp >> 6));
                            out += (char)(0x80 | (cp & 0x3F));
                        } else {
                            out += (char)(0xE0 | (cp >> 12));
                            out += (char)(0x80 | ((cp >> 6) & 0x3F));
                            out += (char)(0x80 | (cp & 0x3F));
                        }
                        break;
                    }
                    default: out += e;
                }
            } else out += c;
        }
        if (i >= s.size()) err("unterminated string");
        i++; // closing quote
        return out;
    }

    Value number() {
        size_t start = i;
        if (i < s.size() && (s[i] == '-' || s[i] == '+')) i++;
        while (i < s.size() && (std::isdigit((unsigned char)s[i]) || s[i]=='.' ||
                                s[i]=='e' || s[i]=='E' || s[i]=='-' || s[i]=='+')) i++;
        if (start == i) err("bad number");
        return vnum(std::stod(s.substr(start, i - start)));
    }

    Value object() {
        i++; // {
        auto m = std::make_shared<ValueMap>();
        ws();
        if (i < s.size() && s[i] == '}') { i++; return vmap(m); }
        for (;;) {
            ws();
            std::string k = strv();
            ws();
            if (i >= s.size() || s[i] != ':') err("expected ':'");
            i++;
            (*m)[k] = value();
            ws();
            if (i < s.size() && s[i] == ',') { i++; continue; }
            if (i < s.size() && s[i] == '}') { i++; break; }
            err("expected ',' or '}'");
        }
        return vmap(m);
    }

    Value array() {
        i++; // [
        auto a = std::make_shared<ValueList>();
        ws();
        if (i < s.size() && s[i] == ']') { i++; return varr(a); }
        for (;;) {
            a->push_back(value());
            ws();
            if (i < s.size() && s[i] == ',') { i++; continue; }
            if (i < s.size() && s[i] == ']') { i++; break; }
            err("expected ',' or ']'");
        }
        return varr(a);
    }
};

inline Value parseJson(const std::string& s) {
    JsonParser p(s);
    return p.parse();
}

} // namespace bi