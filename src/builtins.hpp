#pragma once
#include "value.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string_view>

#ifdef BI_HAVE_CURL
#include <curl/curl.h>
#endif

namespace bi {

// ============================================================
//  Thread-local callback hook (installed by Interpreter)
// ============================================================
inline thread_local std::function<Value(const Value&, ValueList&)> g_callFn;

// ============================================================
//  Helper: register a global native
// ============================================================
inline void def(std::shared_ptr<Env> g, const std::string& name, NativeFn fn) {
    g->define(name, makeNative(std::move(fn)));
}

// ============================================================
//  Small string helpers
// ============================================================
inline std::string upper(const std::string& s) {
    std::string o = s;
    for (auto& c : o) c = (char)std::toupper((unsigned char)c);
    return o;
}
inline std::string lower(const std::string& s) {
    std::string o = s;
    for (auto& c : o) c = (char)std::tolower((unsigned char)c);
    return o;
}
inline std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// ============================================================
//  Property access (map fields, .length, ...)
// ============================================================
inline Value getMember(const Value& o, const std::string& name) {
    if (o.type == Value::MAP) {
        auto& m = *o.mapPtr();
        auto it = m.find(name);
        if (it != m.end()) return it->second;
        if (name == "length") return vint((long long)m.size());
        return vnil();
    }
    if (name == "length") {
        if (o.type == Value::ARR) return vint((long long)o.arrPtr()->size());
        if (o.type == Value::STR) return vint((long long)utf8::length(o.strView()));
    }
    return vnil();
}

// ============================================================
//  Method table — stateless singletons, zero allocation
// ============================================================
namespace methods {

// ---------- string methods ----------
inline const NativeFn s_upper = [](ValueList& a) {
    return vstr(upper(std::string(a[0].strView())));
};
inline const NativeFn s_lower = [](ValueList& a) {
    return vstr(lower(std::string(a[0].strView())));
};
inline const NativeFn s_trim = [](ValueList& a) {
    return vstr(trim(std::string(a[0].strView())));
};
inline const NativeFn s_toNum = [](ValueList& a) {
    return vnum(toNum(a[0]));
};
inline const NativeFn s_toInt = [](ValueList& a) {
    return vint(toInt(a[0]));
};
inline const NativeFn s_contains = [](ValueList& a) {
    return vbool(a[0].strView().find(a[1].strView()) != std::string_view::npos);
};
inline const NativeFn s_startsWith = [](ValueList& a) {
    auto s = a[0].strView();
    auto p = a[1].strView();
    return vbool(s.size() >= p.size() && s.compare(0, p.size(), p) == 0);
};
inline const NativeFn s_endsWith = [](ValueList& a) {
    auto s = a[0].strView();
    auto p = a[1].strView();
    return vbool(s.size() >= p.size() &&
                 s.compare(s.size() - p.size(), p.size(), p) == 0);
};
inline const NativeFn s_indexOf = [](ValueList& a) {
    auto s = a[0].strView();
    auto p = a[1].strView();
    auto pos = s.find(p);
    if (pos == std::string_view::npos) return vint(-1);
    return vint((long long)utf8::length(s.substr(0, pos)));
};
inline const NativeFn s_split = [](ValueList& a) {
    auto list = std::make_shared<ValueList>();
    auto s   = a[0].strView();
    auto sep = a[1].strView();
    if (sep.empty()) {
        for (auto& ch : utf8::chars(s)) list->push_back(vstr(ch));
        return varr(std::move(list));
    }
    size_t start = 0, p;
    while ((p = s.find(sep, start)) != std::string_view::npos) {
        list->push_back(vstr(s.substr(start, p - start)));
        start = p + sep.size();
    }
    list->push_back(vstr(s.substr(start)));
    return varr(std::move(list));
};
inline const NativeFn s_replace = [](ValueList& a) {
    std::string s(a[0].strView());
    std::string_view from = a[1].strView();
    std::string_view to   = a[2].strView();
    if (from.empty()) return vstr(std::move(s));
    size_t p = 0;
    while ((p = s.find(from, p)) != std::string::npos) {
        s.replace(p, from.size(), to);
        p += to.size();
    }
    return vstr(std::move(s));
};
inline const NativeFn s_slice = [](ValueList& a) {
    auto s = a[0].strView();
    long long L = (long long)utf8::length(s);
    long long start = (a.size() > 1) ? toInt(a[1]) : 0;
    long long end   = (a.size() > 2) ? toInt(a[2]) : L;
    if (start < 0) start += L;
    if (end < 0)   end   += L;
    if (start < 0) start = 0;
    if (end > L)   end   = L;
    if (end < start) end = start;
    return vstr(utf8::substr(s, start, end - start));
};
inline const NativeFn s_substr = [](ValueList& a) {
    auto s = a[0].strView();
    long long L = (long long)utf8::length(s);
    long long start = (a.size() > 1) ? toInt(a[1]) : 0;
    long long len   = (a.size() > 2) ? toInt(a[2]) : (L - start);
    return vstr(utf8::substr(s, start, len));
};
inline const NativeFn s_repeat = [](ValueList& a) {
    auto sv = a[0].strView();
    long long n = toInt(a[1]);
    if (n <= 0) return vstr("");
    std::string out;
    out.reserve(sv.size() * (size_t)n);
    for (long long i = 0; i < n; i++) out.append(sv.data(), sv.size());
    return vstr(std::move(out));
};
inline const NativeFn s_charAt = [](ValueList& a) {
    return vstr(utf8::charAt(a[0].strView(), toInt(a[1])));
};

// ---------- array methods ----------
inline const NativeFn a_push = [](ValueList& a) {
    auto& v = *a[0].arrPtr();
    for (size_t i = 1; i < a.size(); i++) v.push_back(std::move(a[i]));
    return vint((long long)v.size());
};
inline const NativeFn a_pop = [](ValueList& a) {
    auto& v = *a[0].arrPtr();
    if (v.empty()) return vnil();
    Value r = std::move(v.back());
    v.pop_back();
    return r;
};
inline const NativeFn a_shift = [](ValueList& a) {
    auto& v = *a[0].arrPtr();
    if (v.empty()) return vnil();
    Value r = std::move(v.front());
    v.erase(v.begin());
    return r;
};
inline const NativeFn a_unshift = [](ValueList& a) {
    auto& v = *a[0].arrPtr();
    for (size_t i = a.size(); i-- > 1;)
        v.insert(v.begin(), std::move(a[i]));
    return vint((long long)v.size());
};
inline const NativeFn a_join = [](ValueList& a) {
    std::string sep = (a.size() > 1) ? std::string(a[1].strView()) : ",";
    std::string out;
    const auto& v = *a[0].arrPtr();
    for (size_t i = 0; i < v.size(); i++) {
        if (i) out += sep;
        out += toStr(v[i]);
    }
    return vstr(std::move(out));
};
inline const NativeFn a_indexOf = [](ValueList& a) {
    const auto& v = *a[0].arrPtr();
    for (size_t i = 0; i < v.size(); i++)
        if (valueEquals(v[i], a[1])) return vint((long long)i);
    return vint(-1);
};
inline const NativeFn a_contains = [](ValueList& a) {
    for (auto& x : *a[0].arrPtr())
        if (valueEquals(x, a[1])) return vbool(true);
    return vbool(false);
};
inline const NativeFn a_slice = [](ValueList& a) {
    auto out = std::make_shared<ValueList>();
    const auto& v = *a[0].arrPtr();
    long long n = (long long)v.size();
    long long s = (a.size() > 1) ? toInt(a[1]) : 0;
    long long e = (a.size() > 2) ? toInt(a[2]) : n;
    if (s < 0) s = n + s;
    if (e < 0) e = n + e;
    s = std::max(0LL, std::min(s, n));
    e = std::max(0LL, std::min(e, n));
    if (e > s) out->reserve((size_t)(e - s));
    for (long long i = s; i < e; i++) out->push_back(v[(size_t)i]);
    return varr(std::move(out));
};
inline const NativeFn a_reverse = [](ValueList& a) {
    auto& v = *a[0].arrPtr();
    std::reverse(v.begin(), v.end());
    return a[0];
};
inline const NativeFn a_map = [](ValueList& a) {
    auto out = std::make_shared<ValueList>();
    const auto& v = *a[0].arrPtr();
    out->reserve(v.size());
    for (size_t i = 0; i < v.size(); i++) {
        ValueList args;
        args.reserve(2);
        args.push_back(v[i]);
        args.push_back(vint((long long)i));
        out->push_back(g_callFn(a[1], args));
    }
    return varr(std::move(out));
};
inline const NativeFn a_filter = [](ValueList& a) {
    auto out = std::make_shared<ValueList>();
    const auto& v = *a[0].arrPtr();
    for (size_t i = 0; i < v.size(); i++) {
        ValueList args;
        args.reserve(2);
        args.push_back(v[i]);
        args.push_back(vint((long long)i));
        if (truthy(g_callFn(a[1], args))) out->push_back(v[i]);
    }
    return varr(std::move(out));
};
inline const NativeFn a_reduce = [](ValueList& a) {
    Value acc = (a.size() > 2) ? a[2] : vnil();
    const auto& v = *a[0].arrPtr();
    for (size_t i = 0; i < v.size(); i++) {
        ValueList args;
        args.reserve(3);
        args.push_back(acc);
        args.push_back(v[i]);
        args.push_back(vint((long long)i));
        acc = g_callFn(a[1], args);
    }
    return acc;
};
inline const NativeFn a_sort = [](ValueList& a) {
    auto& v = *a[0].arrPtr();
    std::sort(v.begin(), v.end(), [](const Value& x, const Value& y) {
        bool xNum = (x.type == Value::INT || x.type == Value::NUM);
        bool yNum = (y.type == Value::INT || y.type == Value::NUM);
        if (xNum && yNum) return toNum(x) < toNum(y);
        return toStr(x) < toStr(y);
    });
    return a[0];
};
inline const NativeFn a_concat = [](ValueList& a) {
    auto out = std::make_shared<ValueList>(*a[0].arrPtr());
    for (size_t i = 1; i < a.size(); i++) {
        if (a[i].type == Value::ARR) {
            const auto& src = *a[i].arrPtr();
            for (auto& x : src) out->push_back(x);
        } else {
            out->push_back(a[i]);
        }
    }
    return varr(std::move(out));
};

// ---------- map methods ----------
inline const NativeFn m_keys = [](ValueList& a) {
    auto out = std::make_shared<ValueList>();
    for (auto& kv : *a[0].mapPtr()) out->push_back(vstr(kv.first));
    return varr(std::move(out));
};
inline const NativeFn m_values = [](ValueList& a) {
    auto out = std::make_shared<ValueList>();
    for (auto& kv : *a[0].mapPtr()) out->push_back(kv.second);
    return varr(std::move(out));
};
inline const NativeFn m_has = [](ValueList& a) {
    return vbool(a[0].mapPtr()->count(std::string(a[1].strView())) > 0);
};
inline const NativeFn m_get = [](ValueList& a) {
    auto& m = *a[0].mapPtr();
    auto it = m.find(std::string(a[1].strView()));
    if (it != m.end()) return it->second;
    return (a.size() > 2) ? a[2] : vnil();
};
inline const NativeFn m_set = [](ValueList& a) {
    (*a[0].mapPtr())[std::string(a[1].strView())] =
        (a.size() > 2) ? a[2] : vnil();
    return a[0];
};
inline const NativeFn m_remove = [](ValueList& a) {
    a[0].mapPtr()->erase(std::string(a[1].strView()));
    return a[0];
};

} // namespace methods

inline const NativeFn* lookupMethod(const Value& o, const std::string& name) {
    using namespace methods;

    if (o.type == Value::STR) {
        if (name == "upper")      return &s_upper;
        if (name == "lower")      return &s_lower;
        if (name == "trim")       return &s_trim;
        if (name == "toNum")      return &s_toNum;
        if (name == "toInt")      return &s_toInt;
        if (name == "contains")   return &s_contains;
        if (name == "startsWith") return &s_startsWith;
        if (name == "endsWith")   return &s_endsWith;
        if (name == "indexOf")    return &s_indexOf;
        if (name == "split")      return &s_split;
        if (name == "replace")    return &s_replace;
        if (name == "slice")      return &s_slice;
        if (name == "substr")     return &s_substr;
        if (name == "repeat")     return &s_repeat;
        if (name == "charAt")     return &s_charAt;
    }
    if (o.type == Value::ARR) {
        if (name == "push")     return &a_push;
        if (name == "pop")      return &a_pop;
        if (name == "shift")    return &a_shift;
        if (name == "unshift")  return &a_unshift;
        if (name == "join")     return &a_join;
        if (name == "indexOf")  return &a_indexOf;
        if (name == "contains") return &a_contains;
        if (name == "slice")    return &a_slice;
        if (name == "reverse")  return &a_reverse;
        if (name == "map")      return &a_map;
        if (name == "filter")   return &a_filter;
        if (name == "reduce")   return &a_reduce;
        if (name == "sort")     return &a_sort;
        if (name == "concat")   return &a_concat;
    }
    if (o.type == Value::MAP) {
        if (name == "keys")     return &m_keys;
        if (name == "values")   return &m_values;
        if (name == "has")      return &m_has;
        if (name == "get")      return &m_get;
        if (name == "set")      return &m_set;
        if (name == "remove" || name == "delete") return &m_remove;
    }
    return nullptr;
}

// ============================================================
//  Test framework (describe / it / expect)
// ============================================================
struct TestState {
    std::vector<std::string> describeStack;
    int passed = 0;
    int failed = 0;
};

inline TestState& testState() {
    static thread_local TestState ts;
    return ts;
}

inline void registerTestFramework(std::shared_ptr<Env> g) {
    def(g, "describe", [](ValueList& a) -> Value {
        if (a.size() < 2 || a[1].type != Value::FUNC)
            throw std::runtime_error("describe(name, fn) requires a function");
        auto& ts = testState();
        ts.describeStack.push_back(std::string(a[0].strView()));
        try {
            ValueList noargs;
            g_callFn(a[1], noargs);
        } catch (...) {
            ts.describeStack.pop_back();
            throw;
        }
        ts.describeStack.pop_back();
        return vnil();
    });

    def(g, "it", [](ValueList& a) -> Value {
        if (a.size() < 2 || a[1].type != Value::FUNC)
            throw std::runtime_error("it(name, fn) requires a function");
        auto& ts = testState();
        std::string full;
        for (auto& s : ts.describeStack) { full += s; full += " > "; }
        full += std::string(a[0].strView());

        try {
            ValueList noargs;
            g_callFn(a[1], noargs);
            ts.passed++;
            std::cout << "    \xE2\x9C\x93 " << full << "\n";
        } catch (std::exception& e) {
            ts.failed++;
            std::cout << "    \xE2\x9C\x97 " << full << "\n        " << e.what() << "\n";
        }
        return vnil();
    });

    def(g, "expect", [](ValueList& a) -> Value {
        Value actual = a.empty() ? vnil() : a[0];
        auto m = std::make_shared<ValueMap>();

        (*m)["toBe"] = makeNative([actual](ValueList& b) {
            if (b.empty() || !valueEquals(actual, b[0]))
                throw std::runtime_error(
                    "expect(" + toStr(actual) + ").toBe(" +
                    (b.empty() ? std::string("?") : toStr(b[0])) + ") failed");
            return vbool(true);
        });
        (*m)["toEqual"] = makeNative([actual](ValueList& b) {
            if (b.empty() || !valueEquals(actual, b[0]))
                throw std::runtime_error(
                    "expect(...).toEqual(...) failed: " +
                    toStr(actual) + " != " +
                    (b.empty() ? std::string("?") : toStr(b[0])));
            return vbool(true);
        });
        (*m)["toBeTruthy"] = makeNative([actual](ValueList&) {
            if (!truthy(actual))
                throw std::runtime_error("expected truthy, got " + toStr(actual));
            return vbool(true);
        });
        (*m)["toBeFalsy"] = makeNative([actual](ValueList&) {
            if (truthy(actual))
                throw std::runtime_error("expected falsy, got " + toStr(actual));
            return vbool(true);
        });
        (*m)["toContain"] = makeNative([actual](ValueList& b) {
            if (b.empty())
                throw std::runtime_error("toContain needs an argument");
            if (actual.type == Value::ARR) {
                for (auto& x : *actual.arrPtr())
                    if (valueEquals(x, b[0])) return vbool(true);
                throw std::runtime_error("array does not contain " + toStr(b[0]));
            }
            if (actual.type == Value::STR) {
                if (actual.strView().find(b[0].strView()) != std::string_view::npos)
                    return vbool(true);
                throw std::runtime_error("string does not contain " + toStr(b[0]));
            }
            throw std::runtime_error("toContain: unsupported type " + typeName(actual));
        });
        (*m)["toThrow"] = makeNative([actual](ValueList&) {
            if (actual.type != Value::FUNC && actual.type != Value::NATIVE)
                throw std::runtime_error("toThrow expects a function");
            try {
                ValueList noargs;
                g_callFn(actual, noargs);
            } catch (...) {
                return vbool(true);
            }
            throw std::runtime_error("expected function to throw, but it didn't");
        });

        return vmap(std::move(m));
    });
}

// ============================================================
//  Global builtins
// ============================================================
inline void registerBuiltins(std::shared_ptr<Env> g) {

    // ---------- core ----------
    def(g, "print", [](ValueList& a) {
        for (size_t i = 0; i < a.size(); i++) {
            if (i) std::cout << ' ';
            std::cout << toStr(a[i]);
        }
        std::cout << '\n';
        return vnil();
    });

        def(g, "input", [](ValueList& a) -> Value {
        if (!a.empty()) std::cout << toStr(a[0]);
        std::cout.flush();
        std::string line;
        if (!std::getline(std::cin, line)) return vnil();
        return vstr(line);
    });

    def(g, "len", [](ValueList& a) {
        if (a.empty()) return vint(0);
        if (a[0].type == Value::STR) return vint((long long)utf8::length(a[0].strView()));
        if (a[0].type == Value::ARR) return vint((long long)a[0].arrPtr()->size());
        if (a[0].type == Value::MAP) return vint((long long)a[0].mapPtr()->size());
        return vint(0);
    });

    def(g, "str", [](ValueList& a) {
        return vstr(a.empty() ? std::string_view{} : std::string_view(toStr(a[0])));
    });
    def(g, "num", [](ValueList& a) {
        return vnum(a.empty() ? 0.0 : toNum(a[0]));
    });
    def(g, "int", [](ValueList& a) {
        return vint(a.empty() ? 0LL : toInt(a[0]));
    });
    def(g, "bool", [](ValueList& a) {
        return vbool(!a.empty() && truthy(a[0]));
    });
    def(g, "type", [](ValueList& a) {
        return vstr(a.empty() ? "null" : typeName(a[0]));
    });
    def(g, "keys", [](ValueList& a) {
        auto out = std::make_shared<ValueList>();
        if (!a.empty() && a[0].type == Value::MAP)
            for (auto& kv : *a[0].mapPtr()) out->push_back(vstr(kv.first));
        return varr(std::move(out));
    });
    def(g, "values", [](ValueList& a) {
        auto out = std::make_shared<ValueList>();
        if (!a.empty() && a[0].type == Value::MAP)
            for (auto& kv : *a[0].mapPtr()) out->push_back(kv.second);
        return varr(std::move(out));
    });
    def(g, "assert", [](ValueList& a) {
        if (a.empty() || !truthy(a[0]))
            throw std::runtime_error(a.size() > 1 ? toStr(a[1]) : "assertion failed");
        return vnil();
    });
    def(g, "exit", [](ValueList& a) -> Value {
        std::exit(a.empty() ? 0 : (int)toInt(a[0]));
        return vnil();
    });

    // ---------- collections ----------
    def(g, "range", [](ValueList& a) {
        long long s = 0, e = 0, st = 1;
        if (a.size() == 1) {
            e = toInt(a[0]);
        } else if (a.size() >= 2) {
            s = toInt(a[0]);
            e = toInt(a[1]);
            if (a.size() > 2) st = toInt(a[2]);
        }
        if (st == 0) st = 1;
        auto out = std::make_shared<ValueList>();
        if (st > 0 && e > s) out->reserve((size_t)((e - s + st - 1) / st));
        else if (st < 0 && e < s) out->reserve((size_t)((s - e - st - 1) / -st));
        for (long long i = s; st > 0 ? i < e : i > e; i += st)
            out->push_back(vint(i));
        return varr(std::move(out));
    });

    def(g, "map", [](ValueList& a) {
        auto out = std::make_shared<ValueList>();
        if (a[0].type != Value::ARR || a.size() < 2) return varr(std::move(out));
        const auto& v = *a[0].arrPtr();
        out->reserve(v.size());
        for (size_t i = 0; i < v.size(); i++) {
            ValueList args;
            args.reserve(2);
            args.push_back(v[i]);
            args.push_back(vint((long long)i));
            out->push_back(g_callFn(a[1], args));
        }
        return varr(std::move(out));
    });
    def(g, "filter", [](ValueList& a) {
        auto out = std::make_shared<ValueList>();
        if (a[0].type != Value::ARR || a.size() < 2) return varr(std::move(out));
        const auto& v = *a[0].arrPtr();
        for (size_t i = 0; i < v.size(); i++) {
            ValueList args;
            args.reserve(2);
            args.push_back(v[i]);
            args.push_back(vint((long long)i));
            if (truthy(g_callFn(a[1], args))) out->push_back(v[i]);
        }
        return varr(std::move(out));
    });
    def(g, "reduce", [](ValueList& a) {
        Value acc = (a.size() > 2) ? a[2] : vnil();
        if (a[0].type != Value::ARR || a.size() < 2) return acc;
        const auto& v = *a[0].arrPtr();
        for (size_t i = 0; i < v.size(); i++) {
            ValueList args;
            args.reserve(3);
            args.push_back(acc);
            args.push_back(v[i]);
            args.push_back(vint((long long)i));
            acc = g_callFn(a[1], args);
        }
        return acc;
    });

    // ---------- strings ----------
    def(g, "split", [](ValueList& a) {
        auto list = std::make_shared<ValueList>();
        auto s   = a[0].strView();
        auto sep = a[1].strView();
        if (sep.empty()) {
            for (auto& ch : utf8::chars(s)) list->push_back(vstr(ch));
            return varr(std::move(list));
        }
        size_t start = 0, p;
        while ((p = s.find(sep, start)) != std::string_view::npos) {
            list->push_back(vstr(s.substr(start, p - start)));
            start = p + sep.size();
        }
        list->push_back(vstr(s.substr(start)));
        return varr(std::move(list));
    });
    def(g, "join", [](ValueList& a) {
        std::string sep = (a.size() > 1) ? std::string(a[1].strView()) : ",";
        std::string out;
        if (a[0].type == Value::ARR) {
            const auto& v = *a[0].arrPtr();
            for (size_t i = 0; i < v.size(); i++) {
                if (i) out += sep;
                out += toStr(v[i]);
            }
        }
        return vstr(std::move(out));
    });
    def(g, "trim", [](ValueList& a) {
        return vstr(trim(std::string(a[0].strView())));
    });
    def(g, "upper", [](ValueList& a) {
        return vstr(upper(std::string(a[0].strView())));
    });
    def(g, "lower", [](ValueList& a) {
        return vstr(lower(std::string(a[0].strView())));
    });
    def(g, "replace", [](ValueList& a) {
        std::string s(a[0].strView());
        std::string_view from = a[1].strView();
        std::string_view to   = a[2].strView();
        if (from.empty()) return vstr(std::move(s));
        size_t p = 0;
        while ((p = s.find(from, p)) != std::string::npos) {
            s.replace(p, from.size(), to);
            p += to.size();
        }
        return vstr(std::move(s));
    });

    // ---------- data ----------
    def(g, "parseJson", [](ValueList& a) {
        if (a.empty()) return vnil();
        return parseJson(std::string(a[0].strView()));
    });
    def(g, "env", [](ValueList& a) {
        if (a.empty()) return vstr("");
        std::string name(a[0].strView());
        const char* v = std::getenv(name.c_str());
        if (v) return vstr(v);
        return (a.size() > 1) ? a[1] : vnil();
    });

    // ---------- time & random ----------
    def(g, "time", [](ValueList&) {
        using namespace std::chrono;
        return vnum((double)duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count() / 1000.0);
    });
    def(g, "random", [](ValueList&) {
        static thread_local std::mt19937_64 rng(std::random_device{}());
        std::uniform_real_distribution<double> d(0.0, 1.0);
        return vnum(d(rng));
    });

    // ---------- files ----------
    def(g, "readFile", [](ValueList& a) {
        std::ifstream f(std::string(a[0].strView()), std::ios::binary);
        if (!f) return vstr("");
        std::stringstream ss;
        ss << f.rdbuf();
        return vstr(std::move(ss).str());
    });
    def(g, "writeFile", [](ValueList& a) {
        std::ofstream f(std::string(a[0].strView()), std::ios::binary);
        if (!f)
            throw std::runtime_error("cannot write file: " +
                                     std::string(a[0].strView()));
        f << a[1].strView();
        return vbool(true);
    });

    // ---------- web response helpers ----------
    def(g, "json", [](ValueList& a) {
        std::string s = a.empty() ? "null" : toJson(a[0]);
        if (currentResponse) {
            currentResponse->contentType = "application/json; charset=utf-8";
            currentResponse->body = s;
        }
        return vstr(std::move(s));
    });
    def(g, "html", [](ValueList& a) {
        std::string s = a.empty() ? std::string("") : toStr(a[0]);
        if (currentResponse) {
            currentResponse->contentType = "text/html; charset=utf-8";
            currentResponse->body = s;
        }
        return vstr(std::move(s));
    });
    def(g, "status", [](ValueList& a) {
        if (currentResponse && !a.empty())
            currentResponse->status = (int)toInt(a[0]);
        return vnil();
    });
    def(g, "header", [](ValueList& a) {
        if (currentResponse && a.size() >= 2)
            currentResponse->headers[std::string(a[0].strView())] =
                std::string(a[1].strView());
        return vnil();
    });
    def(g, "redirect", [](ValueList& a) {
        if (currentResponse) {
            currentResponse->status = 302;
            currentResponse->headers["Location"] =
                a.empty() ? "/" : std::string(a[0].strView());
        }
        return vnil();
    });
    def(g, "serve", [](ValueList& a) -> Value {
        int port = a.empty() ? 8080 : (int)toInt(a[0]);
        throw ServeSignal{port};
    });

    // ---------- HTTP client ----------
#ifdef BI_HAVE_CURL
    def(g, "fetch", [](ValueList& a) -> Value {
        throw std::runtime_error(
            "fetch: HTTP client polish is pending — coming in the next PR");
    });
#else
    def(g, "fetch", [](ValueList&) -> Value {
        throw std::runtime_error("fetch: rebuild with -DBI_HAVE_CURL -lcurl");
    });
#endif

    // ---------- test framework ----------
    registerTestFramework(g);
}

} // namespace bi