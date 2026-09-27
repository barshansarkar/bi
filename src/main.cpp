#include "http.hpp"
#include "interpreter.hpp"
#include "pkg.hpp"

#include <cstring>
#include <iostream>
#include <string>

static const char* VERSION = "0.1.0";

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
        "  init                   Create bi.json + src/main.bi\n"
        "  new <name>             Scaffold a new project\n"
        "  install [pkg]          Install a package\n"
        "  remove <pkg>           Uninstall a package\n"
        "  list                   List installed packages\n"
        "  version                Print version\n"
        "  help                   Print this message\n";
}

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 1; }

    std::string cmd = argv[1];

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

            bi::Interpreter interp;
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

    } catch (std::exception& e) {
        std::cerr << "bi: error: " << e.what() << "\n";
        return 1;
    }
}
