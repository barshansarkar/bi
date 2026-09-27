#include "http.hpp"
#include "interpreter.hpp"
#include "pkg.hpp"
#include "repl.hpp"
#include "test.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

static const char* VERSION = "0.3.1";

static void usage() {
    std::cout <<
        "bi " << VERSION << " - the .bi language\n"
        "\n"
        "USAGE:\n"
        "  bi <command> [options]\n"
        "\n"
        "COMMANDS:\n"
        "  run <file.bi>          Run a .bi program\n"
        "  serve <file.bi>        Run and start the web server\n"
        "      --port <n>         Override the serve() port\n"
        "  repl                   Interactive REPL\n"
        "  test [dir]             Run *.bi tests (default: ./tests)\n"
        "  init                   Create bi.json + src/main.bi\n"
        "  new <name>             Scaffold a new project\n"
        "  install [pkg]          Install a package\n"
        "  remove <pkg>           Uninstall a package\n"
        "  list                   List installed packages\n"
        "  version                Print version\n"
        "  help                   Print this message\n";
}

// ---- pretty error with source snippet ----
static void printLocatedError(const std::string& file, int line, const std::string& msg) {
    std::cerr << "bi: " << msg << "\n";
    if (file.empty() || line <= 0) return;

    std::ifstream f(file);
    if (!f) return;

    std::string src;
    int cur = 0;
    while (std::getline(f, src)) if (++cur == line) break;
    if (cur != line) return;

    while (!src.empty() && (src.back() == '\r' || src.back() == '\n')) src.pop_back();

    std::string lineNum = std::to_string(line);
    std::string pad(lineNum.size(), ' ');
    std::cerr << "  --> " << file << ":" << line << "\n";
    std::cerr << " " << pad << " |\n";
    std::cerr << " " << lineNum << " | " << src << "\n";
    std::cerr << " " << pad << " | ^\n";
}

int main(int argc, char** argv) {
    // Bare `bi` on a TTY → REPL
    if (argc < 2) {
        if (isatty(fileno(stdin))) {
            bi::Interpreter interp;
            return bi::runRepl(interp);
        }
        usage();
        return 1;
    }

    std::string cmd = argv[1];
    bi::Interpreter interp;

    try {
        if (cmd == "run" || cmd == "serve" || cmd == "dev") {
            if (argc < 3) {
                std::cerr << "usage: bi " << cmd << " <file.bi>\n";
                return 1;
            }
            int portOverride = 0;
            for (int i = 3; i < argc; i++) {
                std::string a = argv[i];
                if ((a == "--port" || a == "-p") && i + 1 < argc)
                    portOverride = std::atoi(argv[++i]);
            }

            try {
                interp.runFile(argv[2]);
                std::cout << "bi: program finished (no serve() call)\n";
            } catch (bi::ServeSignal& ss) {
                int port = portOverride ? portOverride : ss.port;
                bi::HttpServer server(interp, port);
                server.run();
            }
            return 0;
        }

        if (cmd == "repl" || cmd == "r") return bi::runRepl(interp);

        if (cmd == "test") {
            std::string dir = (argc > 2) ? argv[2] : "tests";
            return bi::runTests(dir);
        }

        if (cmd == "init")  return bi::Pkg::init();
        if (cmd == "new") {
            if (argc < 3) { std::cerr << "usage: bi new <name>\n"; return 1; }
            return bi::Pkg::newProject(argv[2]);
        }
        if (cmd == "install" || cmd == "i" || cmd == "add") {
            if (argc < 3) return bi::Pkg::installAll();
            int rc = 0;
            for (int i = 2; i < argc; i++)
                if (bi::Pkg::install(argv[i]) != 0) rc = 1;
            return rc;
        }
        if (cmd == "remove" || cmd == "rm" || cmd == "uninstall") {
            if (argc < 3) { std::cerr << "usage: bi remove <pkg>\n"; return 1; }
            return bi::Pkg::remove(argv[2]);
        }
        if (cmd == "list" || cmd == "ls") return bi::Pkg::list();

        if (cmd == "version" || cmd == "--version" || cmd == "-v") {
            std::cout << "bi " << VERSION << "\n";
            return 0;
        }
        if (cmd == "help" || cmd == "--help" || cmd == "-h") {
            usage();
            return 0;
        }

        std::cerr << "bi: unknown command '" << cmd << "'\n\n";
        usage();
        return 1;

    } catch (bi::BiError& e) {
        printLocatedError(interp.currentFile(), e.line, e.what());
        return 1;
    } catch (bi::ThrowSignal& t) {
        printLocatedError(interp.currentFile(), interp.currentLine(),
                          "unhandled throw: " + bi::toStr(t.value));
        return 1;
    } catch (bi::ReturnSignal&) {
        std::cerr << "bi: return outside function\n";
        return 1;
    } catch (std::exception& e) {
        std::cerr << "bi: error: " << e.what() << "\n";
        return 1;
    }
}