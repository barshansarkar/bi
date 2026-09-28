#pragma once
#include "interpreter.hpp"
#include "value.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace bi {

inline int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

inline std::string urlDecode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int hi = hexVal(s[i + 1]), lo = hexVal(s[i + 2]);
            if (hi >= 0 && lo >= 0) { out += (char)(hi * 16 + lo); i += 2; continue; }
        }
        if (s[i] == '+') out += ' ';
        else             out += s[i];
    }
    return out;
}

inline std::vector<std::string> splitPath(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == '/') { if (!cur.empty()) out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

inline bool matchPath(const std::string& pattern, const std::string& path, ValueMap& params) {
    auto ps = splitPath(pattern);
    auto xs = splitPath(path);
    if (ps.size() != xs.size()) return false;
    for (size_t i = 0; i < ps.size(); i++) {
        if (!ps[i].empty() && ps[i][0] == ':') params[ps[i].substr(1)] = vstr(urlDecode(xs[i]));
        else if (ps[i] == "*")                 continue;
        else if (ps[i] != xs[i])               return false;
    }
    return true;
}

inline const char* statusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        default:  return "OK";
    }
}

class HttpServer {
public:
    HttpServer(Interpreter& interp, int port) : interp_(interp), port_(port) {}

    void run() {
        int srv = socket(AF_INET, SOCK_STREAM, 0);
        if (srv < 0) throw std::runtime_error("socket() failed");

        int opt = 1;
        setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons((uint16_t)port_);

        if (bind(srv, (sockaddr*)&addr, sizeof(addr)) < 0)
            throw std::runtime_error("bind() failed on port " + std::to_string(port_));
        if (listen(srv, 128) < 0)
            throw std::runtime_error("listen() failed");

        std::cout << "bi: .bi server listening on http://localhost:" << port_ << "\n";
        std::cout << "bi: " << interp_.routes().size() << " route(s) registered\n";
        std::cout << "bi: NOTE: requests are serialized (Interpreter is not reentrant)\n";
        std::cout.flush();

        for (;;) {
            sockaddr_in cli{};
            socklen_t len = sizeof(cli);
            int fd = accept(srv, (sockaddr*)&cli, &len);
            if (fd < 0) continue;

            timeval tv{};
            tv.tv_sec  = 30;
            tv.tv_usec = 0;
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

            std::thread([this, fd] { handle(fd); }).detach();
        }
    }

private:
    Interpreter&    interp_;
    int             port_;
    std::mutex      mtx_;

    static constexpr size_t kMaxHeaderBytes = 64 * 1024;
    static constexpr size_t kMaxBodyBytes   = 1ULL << 30;

    static void sendAll(int fd, const std::string& data) {
        size_t sent = 0;
        while (sent < data.size()) {
            ssize_t n = send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
            if (n <= 0) break;
            sent += (size_t)n;
        }
    }

    static void sendErrAndClose(int fd, int status, const std::string& msg) {
        std::ostringstream r;
        r << "HTTP/1.1 " << status << " " << statusText(status) << "\r\n"
          << "Content-Type: text/plain; charset=utf-8\r\n"
          << "Content-Length: " << msg.size() << "\r\n"
          << "Connection: close\r\n\r\n"
          << msg;
        sendAll(fd, r.str());
        close(fd);
    }

    static void trimInPlace(std::string& s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        size_t b = s.find_last_not_of(" \t\r\n");
        if (a == std::string::npos) { s.clear(); return; }
        s = s.substr(a, b - a + 1);
    }

    void handle(int fd) {
        std::string buf;
        char tmp[8192];

        size_t headerEnd = std::string::npos;
        size_t scanned   = 0;
        for (;;) {
            headerEnd = buf.find("\r\n\r\n", scanned);
            if (headerEnd != std::string::npos) break;

            if (buf.size() > kMaxHeaderBytes) {
                sendErrAndClose(fd, 431, "request header too large");
                return;
            }
            scanned = buf.size() >= 3 ? buf.size() - 3 : 0;

            ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
            if (n <= 0) { close(fd); return; }
            buf.append(tmp, (size_t)n);
        }

        std::string head = buf.substr(0, headerEnd);
        std::string body = buf.substr(headerEnd + 4);

        size_t lineEnd = head.find("\r\n");
        std::string reqLine = head.substr(0, lineEnd);
        std::istringstream rl(reqLine);
        std::string method, target, version;
        if (!(rl >> method >> target >> version)) {
            sendErrAndClose(fd, 400, "malformed request line");
            return;
        }

        std::map<std::string, std::string> headers;
        size_t pos = (lineEnd == std::string::npos) ? head.size() : lineEnd + 2;
        while (pos < head.size()) {
            size_t e = head.find("\r\n", pos);
            if (e == std::string::npos) e = head.size();
            std::string line = head.substr(pos, e - pos);
            pos = e + 2;
            size_t c = line.find(':');
            if (c != std::string::npos) {
                std::string k = line.substr(0, c);
                std::string v = line.substr(c + 1);
                trimInPlace(k);
                trimInPlace(v);
                headers[k] = v;
            }
        }

        size_t contentLen = 0;
        for (auto& kv : headers) {
            std::string k = kv.first;
            for (auto& ch : k) ch = (char)std::tolower((unsigned char)ch);
            if (k == "content-length") {
                try {
                    unsigned long long n = std::stoull(kv.second);
                    if (n > kMaxBodyBytes) {
                        sendErrAndClose(fd, 413, "payload too large");
                        return;
                    }
                    contentLen = (size_t)n;
                } catch (...) {
                    sendErrAndClose(fd, 400, "bad Content-Length");
                    return;
                }
            }
        }

        while (body.size() < contentLen) {
            ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
            if (n <= 0) break;
            body.append(tmp, (size_t)n);
        }
        if (body.size() > contentLen) body = body.substr(0, contentLen);

        std::string path = target, rawQuery;
        size_t q = target.find('?');
        if (q != std::string::npos) { path = target.substr(0, q); rawQuery = target.substr(q + 1); }

        ValueMap queryMap;
        if (!rawQuery.empty()) {
            size_t start = 0;
            while (start <= rawQuery.size()) {
                size_t amp = rawQuery.find('&', start);
                if (amp == std::string::npos) amp = rawQuery.size();
                std::string pair = rawQuery.substr(start, amp - start);
                size_t eq = pair.find('=');
                if (eq == std::string::npos) queryMap[urlDecode(pair)] = vstr("");
                else queryMap[urlDecode(pair.substr(0, eq))] = vstr(urlDecode(pair.substr(eq + 1)));
                if (amp == rawQuery.size()) break;
                start = amp + 1;
            }
        }

        path = urlDecode(path);

        Response resp;
        std::string out = dispatch(method, path, body, headers, queryMap, resp);

        if (resp.body.empty()) resp.body = out;

        std::ostringstream r;
        r << "HTTP/1.1 " << resp.status << " " << statusText(resp.status) << "\r\n";
        r << "Content-Type: " << resp.contentType << "\r\n";
        r << "Content-Length: " << resp.body.size() << "\r\n";
        r << "Connection: close\r\n";
        for (auto& kv : resp.headers) r << kv.first << ": " << kv.second << "\r\n";
        r << "\r\n";
        r << resp.body;

        sendAll(fd, r.str());
        close(fd);
    }

    std::string dispatch(const std::string& method,
                         const std::string& path,
                         const std::string& body,
                         const std::map<std::string, std::string>& headers,
                         ValueMap& queryMap,
                         Response& resp)
    {
        std::lock_guard<std::mutex> lk(mtx_);

        for (auto& fn : interp_.routes()) {
            if (fn->routeMethod != method && fn->routeMethod != "*") continue;
            ValueMap params;
            if (!matchPath(fn->routePath, path, params)) continue;

            Value out = runRoute(fn, params, method, path, body, headers, queryMap, resp);
            return renderValue(out, resp);
        }

        resp.status      = 404;
        resp.contentType = "text/html; charset=utf-8";
        return "<h1>404 Not Found</h1><p>No route for <code>" + path + "</code></p>";
    }

    Value runRoute(std::shared_ptr<Function> fn,
                   ValueMap& params,
                   const std::string& method,
                   const std::string& path,
                   const std::string& body,
                   const std::map<std::string, std::string>& headers,
                   ValueMap& queryMap,
                   Response& resp)
    {
        auto env = fn->closure->child();

        auto hdr = std::make_shared<ValueMap>();
        for (auto& kv : headers) (*hdr)[kv.first] = vstr(kv.second);

        auto reqMap = std::make_shared<ValueMap>();
        (*reqMap)["method"]  = vstr(method);
        (*reqMap)["path"]    = vstr(path);
        (*reqMap)["body"]    = vstr(body);
        (*reqMap)["headers"] = vmap(hdr);
        (*reqMap)["query"]   = vmap(std::make_shared<ValueMap>(queryMap));

        env->define("req",    vmap(reqMap));
        env->define("params", vmap(std::make_shared<ValueMap>(params)));
        env->define("query",  vmap(std::make_shared<ValueMap>(queryMap)));
        env->define("body",   vstr(body));
        env->define("method", vstr(method));
        env->define("path",   vstr(path));

        Response* prev = currentResponse;
        currentResponse = &resp;

        Value out;
        try {
            for (auto& s : fn->body) interp_.exec(s, env);
        } catch (ReturnSignal& rs) {
            out = std::move(rs.value);
        } catch (BreakSignal&) {
            currentResponse = prev;
            resp.status      = 500;
            resp.contentType = "text/plain; charset=utf-8";
            resp.body        = "route handler: 'break' outside loop";
            return vnil();
        } catch (ContinueSignal&) {
            currentResponse = prev;
            resp.status      = 500;
            resp.contentType = "text/plain; charset=utf-8";
            resp.body        = "route handler: 'continue' outside loop";
            return vnil();
        } catch (ThrowSignal& t) {
            currentResponse = prev;
            resp.status      = 500;
            resp.contentType = "text/html; charset=utf-8";
            resp.body = std::string("<h1>500 Internal Server Error</h1><pre>")
                      + toStr(t.value) + "</pre>";
            return vnil();
        } catch (BiError& e) {
            currentResponse = prev;
            resp.status      = 500;
            resp.contentType = "text/html; charset=utf-8";
            resp.body = std::string("<h1>500 Internal Server Error</h1><pre>")
                      + e.what() + "</pre>";
            if (e.line > 0) {
                resp.body += "<p>at " + e.file + ":" + std::to_string(e.line) + "</p>";
                for (auto& fr : e.trace)
                    resp.body += "<p>  at " + fr.function + " (" +
                                 fr.file + ":" + std::to_string(fr.line) + ")</p>";
            }
            return vnil();
        } catch (std::exception& ex) {
            currentResponse = prev;
            resp.status      = 500;
            resp.contentType = "text/html; charset=utf-8";
            resp.body = std::string("<h1>500 Internal Server Error</h1><pre>")
                      + ex.what() + "</pre>";
            return vnil();
        }
        currentResponse = prev;
        return out;
    }

    std::string renderValue(const Value& v, Response& resp) {
        if (v.type == Value::NIL) return resp.body;
        if (v.type == Value::STR) return std::string(v.strView());   // ← FIX
        if (v.type == Value::ARR || v.type == Value::MAP) {
            if (resp.contentType.find("json") == std::string::npos)
                resp.contentType = "application/json; charset=utf-8";
            return toJson(v);
        }
        return toStr(v);
    }
};

} // namespace bi