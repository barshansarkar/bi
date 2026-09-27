#pragma once
#include "value.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <sstream>

#ifdef BI_HAVE_CURL
#include <curl/curl.h>
#endif

namespace bi {

// Per-thread so that concurrently running routes don't clobber each other.
inline thread_local std::function<Value(const Value&, ValueList&)> g_callFn;

inline Value makeNative(NativeFn fn) {
    Value v;
    v.type = Value::NATIVE;
    v.native = std::move(fn);
    return v;
}

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

// ---------------- member access ----------------

inline Value getMember(const Value& o, const std::string& name) {
    if (name == "length") {
        if (o.type == Value::ARR) return vnum((double)o.arr->size());
        if (o.type == Value::STR) return vnum((double)o.str.size());
        if (o.type == Value::MAP) return vnum((double)o.map->size());
    }
    if (o.type == Value::MAP) {
        auto it = o.map->find(name);
        return it != o.map->end() ? it->second : vnil();
    }
    return vnil();
}

inline Value getMethod(const Value& o, const std::string& name) {
    // ---------- string methods ----------
    if (o.type == Value::STR) {
        if (name == "upper")     return makeNative([](ValueList& a){ return vstr(upper(toStr(a[0]))); });
        if (name == "lower")     return makeNative([](ValueList& a){ return vstr(lower(toStr(a[0]))); });
        if (name == "trim")      return makeNative([](ValueList& a){ return vstr(trim(toStr(a[0]))); });
        if (name == "toNum")     return makeNative([](ValueList& a){ return vnum(toNum(a[0])); });
        if (name == "contains")  return makeNative([](ValueList& a){
            return vbool(toStr(a[0]).find(toStr(a[1])) != std::string::npos); });
        if (name == "startsWith") return makeNative([](ValueList& a){
            const std::string s = toStr(a[0]), p = toStr(a[1]);
            return vbool(s.size() >= p.size() && s.compare(0, p.size(), p) == 0); });
        if (name == "endsWith") return makeNative([](ValueList& a){
            const std::string s = toStr(a[0]), p = toStr(a[1]);
            return vbool(s.size() >= p.size() && s.compare(s.size()-p.size(), p.size(), p) == 0); });
        if (name == "indexOf") return makeNative([](ValueList& a){
            auto p = toStr(a[0]).find(toStr(a[1]));
            return vnum(p == std::string::npos ? -1 : (double)p); });
        if (name == "split") return makeNative([](ValueList& a){
            auto list = std::make_shared<ValueList>();
            std::string s = toStr(a[0]), sep = toStr(a[1]);
            if (sep.empty()) { for (char c : s) list->push_back(vstr(std::string(1,c))); return varr(list); }
            size_t start = 0, p;
            while ((p = s.find(sep, start)) != std::string::npos) {
                list->push_back(vstr(s.substr(start, p - start)));
                start = p + sep.size();
            }
            list->push_back(vstr(s.substr(start)));
            return varr(list); });
        if (name == "replace") return makeNative([](ValueList& a){
            std::string s = toStr(a[0]), from = toStr(a[1]), to = toStr(a[2]);
            if (from.empty()) return vstr(s);
            size_t p = 0;
            while ((p = s.find(from, p)) != std::string::npos) {
                s.replace(p, from.size(), to);
                p += to.size();
            }
            return vstr(s); });
        if (name == "substr" || name == "slice") return makeNative([](ValueList& a){
            std::string s = toStr(a[0]);
            long long start = a.size() > 1 ? (long long)toNum(a[1]) : 0;
            long long len   = a.size() > 2 ? (long long)toNum(a[2]) : (long long)s.size();
            if (start < 0) start = (long long)s.size() + start;
            if (start < 0) start = 0;
            if (start > (long long)s.size()) return vstr("");
            if (a.size() <= 2) len = (long long)s.size() - start;
            return vstr(s.substr((size_t)start, (size_t)len)); });
        if (name == "repeat") return makeNative([](ValueList& a){
            std::string s = toStr(a[0]); int n = (int)toNum(a[1]); std::string o;
            for (int i = 0; i < n; i++) o += s;
            return vstr(o); });
        if (name == "charAt") return makeNative([](ValueList& a){
            std::string s = toStr(a[0]); int i = (int)toNum(a[1]);
            if (i < 0 || i >= (int)s.size()) return vstr("");
            return vstr(std::string(1, s[i])); });
    }

    // ---------- array methods ----------
    if (o.type == Value::ARR) {
        if (name == "push") return makeNative([](ValueList& a){
            for (size_t i = 1; i < a.size(); i++) a[0].arr->push_back(a[i]);
            return vnum((double)a[0].arr->size()); });
        if (name == "pop") return makeNative([](ValueList& a){
            if (a[0].arr->empty()) return vnil();
            Value v = a[0].arr->back(); a[0].arr->pop_back(); return v; });
        if (name == "shift") return makeNative([](ValueList& a){
            if (a[0].arr->empty()) return vnil();
            Value v = a[0].arr->front(); a[0].arr->erase(a[0].arr->begin()); return v; });
        if (name == "unshift") return makeNative([](ValueList& a){
            for (size_t i = a.size(); i-- > 1;)
                a[0].arr->insert(a[0].arr->begin(), a[i]);
            return vnum((double)a[0].arr->size()); });
        if (name == "join") return makeNative([](ValueList& a){
            std::string sep = a.size() > 1 ? toStr(a[1]) : ",";
            std::string out;
            for (size_t i = 0; i < a[0].arr->size(); i++) {
                if (i) out += sep;
                out += toStr((*a[0].arr)[i]);
            }
            return vstr(out); });
        if (name == "indexOf") return makeNative([](ValueList& a){
            for (size_t i = 0; i < a[0].arr->size(); i++)
                if (valueEquals((*a[0].arr)[i], a[1])) return vnum((double)i);
            return vnum(-1); });
        if (name == "contains") return makeNative([](ValueList& a){
            for (auto& x : *a[0].arr) if (valueEquals(x, a[1])) return vbool(true);
            return vbool(false); });
        if (name == "slice") return makeNative([](ValueList& a){
            auto out = std::make_shared<ValueList>();
            long long n = (long long)a[0].arr->size();
            long long s = a.size() > 1 ? (long long)toNum(a[1]) : 0;
            long long e = a.size() > 2 ? (long long)toNum(a[2]) : n;
            if (s < 0) s = n + s;
            if (e < 0) e = n + e;
            s = std::max(0LL, std::min(s, n));
            e = std::max(0LL, std::min(e, n));
            for (long long i = s; i < e; i++) out->push_back((*a[0].arr)[(size_t)i]);
            return varr(out); });
        if (name == "reverse") return makeNative([](ValueList& a){
            std::reverse(a[0].arr->begin(), a[0].arr->end());
            return a[0]; });
        if (name == "map") return makeNative([](ValueList& a){
            auto out = std::make_shared<ValueList>();
            for (size_t i = 0; i < a[0].arr->size(); i++) {
                ValueList args = { (*a[0].arr)[i], vnum((double)i) };
                out->push_back(g_callFn(a[1], args));
            }
            return varr(out); });
        if (name == "filter") return makeNative([](ValueList& a){
            auto out = std::make_shared<ValueList>();
            for (size_t i = 0; i < a[0].arr->size(); i++) {
                ValueList args = { (*a[0].arr)[i], vnum((double)i) };
                if (truthy(g_callFn(a[1], args))) out->push_back((*a[0].arr)[i]);
            }
            return varr(out); });
        if (name == "reduce") return makeNative([](ValueList& a){
            Value acc = a.size() > 2 ? a[2] : vnil();
            for (size_t i = 0; i < a[0].arr->size(); i++) {
                ValueList args = { acc, (*a[0].arr)[i], vnum((double)i) };
                acc = g_callFn(a[1], args);
            }
            return acc; });
        if (name == "sort") return makeNative([](ValueList& a){
            std::sort(a[0].arr->begin(), a[0].arr->end(), [](const Value& x, const Value& y){
                if (x.type == Value::NUM && y.type == Value::NUM) return x.num < y.num;
                return toStr(x) < toStr(y);
            });
            return a[0]; });
        if (name == "concat") return makeNative([](ValueList& a){
            auto out = std::make_shared<ValueList>(*a[0].arr);
            for (size_t i = 1; i < a.size(); i++) {
                if (a[i].type == Value::ARR)
                    for (auto& x : *a[i].arr) out->push_back(x);
                else out->push_back(a[i]);
            }
            return varr(out); });
    }

    // ---------- map methods ----------
    if (o.type == Value::MAP) {
        if (name == "keys") return makeNative([](ValueList& a){
            auto out = std::make_shared<ValueList>();
            for (auto& kv : *a[0].map) out->push_back(vstr(kv.first));
            return varr(out); });
        if (name == "values") return makeNative([](ValueList& a){
            auto out = std::make_shared<ValueList>();
            for (auto& kv : *a[0].map) out->push_back(kv.second);
            return varr(out); });
        if (name == "has") return makeNative([](ValueList& a){
            return vbool(a[0].map->count(toStr(a[1])) > 0); });
        if (name == "get") return makeNative([](ValueList& a){
            auto it = a[0].map->find(toStr(a[1]));
            if (it != a[0].map->end()) return it->second;
            return a.size() > 2 ? a[2] : vnil(); });
        if (name == "set") return makeNative([](ValueList& a){
            (*a[0].map)[toStr(a[1])] = a.size() > 2 ? a[2] : vnil();
            return a[0]; });
        if (name == "remove" || name == "delete") return makeNative([](ValueList& a){
            a[0].map->erase(toStr(a[1])); return a[0]; });
    }

    return vnil();
}

// ---------------- libcurl helper ----------------

#ifdef BI_HAVE_CURL
inline size_t curlWriteCb(char* p, size_t s, size_t n, void* u) {
    static_cast<std::string*>(u)->append(p, s * n);
    return s * n;
}

inline bool httpFetch(const std::string& url,
                      const std::string& method,
                      const std::string& reqBody,
                      const std::map<std::string, std::string>& headers,
                      std::string& outBody,
                      long& outStatus)
{
    CURL* c = curl_easy_init();
    if (!c) return false;

    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWriteCb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &outBody);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "bi/0.3");

    struct curl_slist* hdrList = nullptr;
    for (auto& kv : headers)
        hdrList = curl_slist_append(hdrList, (kv.first + ": " + kv.second).c_str());
    if (hdrList) curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrList);

    if (method == "POST") {
        curl_easy_setopt(c, CURLOPT_POST, 1L);
        curl_easy_setopt(c, CURLOPT_POSTFIELDS, reqBody.c_str());
    } else if (method == "PUT") {
        curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(c, CURLOPT_POSTFIELDS, reqBody.c_str());
    } else if (method != "GET") {
        curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, method.c_str());
        if (!reqBody.empty())
            curl_easy_setopt(c, CURLOPT_POSTFIELDS, reqBody.c_str());
    }

    CURLcode rc = curl_easy_perform(c);
    if (rc == CURLE_OK)
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &outStatus);

    if (hdrList) curl_slist_free_all(hdrList);
    curl_easy_cleanup(c);
    return rc == CURLE_OK;
}
#endif

// ---------------- global builtins ----------------

inline void def(std::shared_ptr<Env> g, const std::string& name, NativeFn fn) {
    g->define(name, makeNative(std::move(fn)));
}

inline void registerBuiltins(std::shared_ptr<Env> g) {

    // --- I/O & core ---
    def(g, "print", [](ValueList& a) {
        for (size_t i = 0; i < a.size(); i++) { if (i) std::cout << ' '; std::cout << toStr(a[i]); }
        std::cout << '\n';
        return vnil();
    });
    def(g, "len",   [](ValueList& a){ return vnum(a.empty() ? 0 : (double)toStr(a[0]).size()); });
    def(g, "str",   [](ValueList& a){ return vstr(a.empty() ? "" : toStr(a[0])); });
    def(g, "num",   [](ValueList& a){ return vnum(a.empty() ? 0 : toNum(a[0])); });
    def(g, "bool",  [](ValueList& a){ return vbool(!a.empty() && truthy(a[0])); });
    def(g, "type",  [](ValueList& a){ return vstr(a.empty() ? "null" : typeName(a[0])); });
    def(g, "keys",  [](ValueList& a){
        auto out = std::make_shared<ValueList>();
        if (!a.empty() && a[0].type == Value::MAP)
            for (auto& kv : *a[0].map) out->push_back(vstr(kv.first));
        return varr(out); });
    def(g, "values", [](ValueList& a){
        auto out = std::make_shared<ValueList>();
        if (!a.empty() && a[0].type == Value::MAP)
            for (auto& kv : *a[0].map) out->push_back(kv.second);
        return varr(out); });
    def(g, "assert", [](ValueList& a){
        if (a.empty() || !truthy(a[0]))
            throw std::runtime_error(a.size() > 1 ? toStr(a[1]) : "assertion failed");
        return vnil(); });
    def(g, "exit", [](ValueList& a) -> Value {
        std::exit(a.empty() ? 0 : (int)toNum(a[0]));
        return vnil();
    });

    // --- collections ---
    def(g, "range", [](ValueList& a){
        double s = 0, e = 0, st = 1;
        if (a.size() == 1) { e = toNum(a[0]); }
        else if (a.size() >= 2) { s = toNum(a[0]); e = toNum(a[1]); if (a.size() > 2) st = toNum(a[2]); }
        auto out = std::make_shared<ValueList>();
        if (st == 0) st = 1;
        for (double i = s; st > 0 ? i < e : i > e; i += st) out->push_back(vnum(i));
        return varr(out); });
    def(g, "map", [](ValueList& a){
        auto out = std::make_shared<ValueList>();
        if (a[0].type != Value::ARR) return varr(out);
        for (size_t i = 0; i < a[0].arr->size(); i++) {
            ValueList args = { (*a[0].arr)[i], vnum((double)i) };
            out->push_back(g_callFn(a[1], args));
        }
        return varr(out); });
    def(g, "filter", [](ValueList& a){
        auto out = std::make_shared<ValueList>();
        if (a[0].type != Value::ARR) return varr(out);
        for (size_t i = 0; i < a[0].arr->size(); i++) {
            ValueList args = { (*a[0].arr)[i], vnum((double)i) };
            if (truthy(g_callFn(a[1], args))) out->push_back((*a[0].arr)[i]);
        }
        return varr(out); });
    def(g, "reduce", [](ValueList& a){
        Value acc = a.size() > 2 ? a[2] : vnil();
        if (a[0].type != Value::ARR) return acc;
        for (size_t i = 0; i < a[0].arr->size(); i++) {
            ValueList args = { acc, (*a[0].arr)[i], vnum((double)i) };
            acc = g_callFn(a[1], args);
        }
        return acc; });

    // --- strings ---
    def(g, "split",   [](ValueList& a){
        auto list = std::make_shared<ValueList>();
        std::string s = toStr(a[0]), sep = toStr(a[1]);
        if (sep.empty()) { for (char c : s) list->push_back(vstr(std::string(1,c))); return varr(list); }
        size_t start = 0, p;
        while ((p = s.find(sep, start)) != std::string::npos) {
            list->push_back(vstr(s.substr(start, p - start)));
            start = p + sep.size();
        }
        list->push_back(vstr(s.substr(start)));
        return varr(list); });
    def(g, "join",    [](ValueList& a){
        std::string sep = a.size() > 1 ? toStr(a[1]) : ",";
        std::string out;
        if (a[0].type == Value::ARR)
            for (size_t i = 0; i < a[0].arr->size(); i++) {
                if (i) out += sep;
                out += toStr((*a[0].arr)[i]);
            }
        return vstr(out); });
    def(g, "trim",    [](ValueList& a){ return vstr(trim(toStr(a[0]))); });
    def(g, "upper",   [](ValueList& a){ return vstr(upper(toStr(a[0]))); });
    def(g, "lower",   [](ValueList& a){ return vstr(lower(toStr(a[0]))); });
    def(g, "replace", [](ValueList& a){
        std::string s = toStr(a[0]), from = toStr(a[1]), to = toStr(a[2]);
        if (from.empty()) return vstr(s);
        size_t p = 0;
        while ((p = s.find(from, p)) != std::string::npos) { s.replace(p, from.size(), to); p += to.size(); }
        return vstr(s); });

    // --- data ---
    def(g, "parseJson", [](ValueList& a){
        if (a.empty()) return vnil();
        return parseJson(toStr(a[0]));   // bi::parseJson from value.hpp
    });
    def(g, "env", [](ValueList& a){
        if (a.empty()) return vstr("");
        const char* v = std::getenv(toStr(a[0]).c_str());
        if (v) return vstr(v);
        return a.size() > 1 ? a[1] : vnil();
    });

    // --- time & random ---
    def(g, "time", [](ValueList&){
        using namespace std::chrono;
        return vnum((double)duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count() / 1000.0); });
    def(g, "random", [](ValueList&){
        static thread_local std::mt19937_64 rng(std::random_device{}());
        std::uniform_real_distribution<double> d(0.0, 1.0);
        return vnum(d(rng)); });

    // --- files ---
    def(g, "readFile", [](ValueList& a){
        std::ifstream f(toStr(a[0]), std::ios::binary);
        if (!f) return vstr("");
        std::stringstream ss; ss << f.rdbuf();
        return vstr(ss.str()); });
    def(g, "writeFile", [](ValueList& a){
        std::ofstream f(toStr(a[0]), std::ios::binary);
        if (!f) throw std::runtime_error("cannot write file: " + toStr(a[0]));
        f << toStr(a[1]);
        return vbool(true); });

    // --- web ---
    def(g, "json", [](ValueList& a){
        std::string s = a.empty() ? "null" : toJson(a[0]);
        if (currentResponse) {
            currentResponse->contentType = "application/json; charset=utf-8";
            currentResponse->body = s;
        }
        return vstr(s); });
    def(g, "html", [](ValueList& a){
        std::string s = a.empty() ? "" : toStr(a[0]);
        if (currentResponse) {
            currentResponse->contentType = "text/html; charset=utf-8";
            currentResponse->body = s;
        }
        return vstr(s); });
    def(g, "status", [](ValueList& a){
        if (currentResponse && !a.empty()) currentResponse->status = (int)toNum(a[0]);
        return vnil(); });
    def(g, "header", [](ValueList& a){
        if (currentResponse && a.size() >= 2)
            currentResponse->headers[toStr(a[0])] = toStr(a[1]);
        return vnil(); });
    def(g, "redirect", [](ValueList& a){
        if (currentResponse) {
            currentResponse->status = 302;
            currentResponse->headers["Location"] = a.empty() ? "/" : toStr(a[0]);
        }
        return vnil(); });
    def(g, "serve", [](ValueList& a) -> Value {
        int port = a.empty() ? 8080 : (int)toNum(a[0]);
        throw ServeSignal{port};
        return vnil(); // unreachable
    });

    // --- HTTP client ---
#ifdef BI_HAVE_CURL
    def(g, "fetch", [](ValueList& a){
        if (a.empty()) throw std::runtime_error("fetch: url required");
        std::string url    = toStr(a[0]);
        std::string method = "GET";
        std::string body;
        std::map<std::string, std::string> headers;

        if (a.size() > 1 && a[1].type == Value::MAP) {
            auto& opts = *a[1].map;
            auto it = opts.find("method");
            if (it != opts.end()) method = toStr(it->second);
            it = opts.find("body");
            if (it != opts.end()) body = toStr(it->second);
            it = opts.find("headers");
            if (it != opts.end() && it->second.type == Value::MAP)
                for (auto& kv : *it->second.map)
                    headers[kv.first] = toStr(kv.second);
        }

        std::string respBody;
        long status = 0;
        if (!httpFetch(url, method, body, headers, respBody, status))
            throw std::runtime_error("fetch failed: " + url);

        auto m = std::make_shared<ValueMap>();
        (*m)["status"] = vnum((double)status);
        (*m)["body"]   = vstr(respBody);
        (*m)["ok"]     = vbool(status >= 200 && status < 300);
        return vmap(m);
    });
#else
    def(g, "fetch", [](ValueList&) -> Value {
        throw std::runtime_error("fetch: rebuild with -DBI_HAVE_CURL -lcurl");
    });
#endif
}

} // namespace bi