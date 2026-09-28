#pragma once
#include "interpreter.hpp"
#include <cctype>
#include <iostream>
#include <string>

namespace bi {

// Returns brace/paren depth, ignoring strings and line comments.
inline int balanceDelta(const std::string& s) {
    int depth = 0;
    bool inStr = false;
    char q = 0;
    bool lineComment = false;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (lineComment) {
            if (c == '\n') lineComment = false;
            continue;
        }
        if (inStr) {
            if (c == '\\') { i++; continue; }
            if (c == q)   inStr = false;
            continue;
        }
        if (c == '"' || c == '\'') { inStr = true; q = c; continue; }
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
            lineComment = true; i++; continue;
        }
        if (c == '{' || c == '(' || c == '[') depth++;
        if (c == '}' || c == ')' || c == ']') depth--;
    }
    return depth;
}

inline int runRepl(Interpreter& interp) {
    std::cout << "bi repl — :help for tips, Ctrl-D to exit\n";
    std::string buf;

    for (;;) {
        std::cout << (buf.empty() ? "> " : "| ");
        std::cout.flush();

        std::string line;
        if (!std::getline(std::cin, line)) { std::cout << "\n"; return 0; }

        // REPL meta-commands (only when not mid-block)
        if (buf.empty() && !line.empty() && line[0] == ':') {
            if (line == ":quit" || line == ":q" || line == ":exit") return 0;
            if (line == ":help" || line == ":h") {
                std::cout << "  :help, :h      this message\n"
                             "  :quit, :q      exit\n"
                             "  :globals       dump current globals\n";
                continue;
            }
                        if (line == ":globals") {
                for (auto& kv : interp.globals()->vars)
                    std::cout << "  " << kv.first << " = " << toStr(kv.second) << "\n";
                continue;
            }
            std::cout << "bi: unknown command " << line << "\n";
            continue;
        }

        buf += line;
        buf += '\n';

        int d = balanceDelta(buf);
        if (d > 0) continue;                        // keep reading
        if (d < 0) {
            std::cerr << "bi: unexpected closing delimiter\n";
            buf.clear();
            continue;
        }

        bool onlyWs = true;
        for (char c : buf) if (!std::isspace((unsigned char)c)) { onlyWs = false; break; }
        if (onlyWs) { buf.clear(); continue; }

        try {
            auto result = interp.runSourceRepl(buf, "<repl>");
            if (result.first && result.second.type != Value::NIL)
                std::cout << toStr(result.second) << "\n";
        } catch (ReturnSignal&) {
            // top-level return: swallow
        } catch (BreakSignal&) {
            std::cerr << "bi: break outside loop\n";
        } catch (ContinueSignal&) {
            std::cerr << "bi: continue outside loop\n";
        } catch (ThrowSignal& t) {
            std::cerr << "bi: unhandled throw: " << toStr(t.value) << "\n";
        } catch (ServeSignal& ss) {
            std::cout << "bi: (serve(" << ss.port << ") ignored in repl)\n";
        } catch (BiError& e) {
            std::cerr << "bi: " << e.what() << "\n";
        } catch (std::exception& e) {
            std::cerr << "bi: " << e.what() << "\n";
        }
        buf.clear();
    }
}

} // namespace bi