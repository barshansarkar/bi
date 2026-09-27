#pragma once
#include "value.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

namespace bi {

inline bool fileExists(const std::string& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}

inline std::string readWholeFile(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return "";
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

inline void writeWholeFile(const std::string& p, const std::string& c) {
    std::ofstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("cannot write " + p);
    f << c;
}

inline std::string defaultRegistry() {
    const char* v = std::getenv("BI_REGISTRY");
    return v ? std::string(v) : std::string("http://localhost:8787");
}

inline std::string shellCapture(const std::string& cmd) {
    std::string out;
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return out;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), p)) > 0) out.append(buf, n);
    pclose(p);
    return out;
}

class Pkg {
public:
    // ---- bi init ----
    static int init() {
        if (fileExists("bi.json")) {
            std::cout << "bi: bi.json already exists\n";
            return 1;
        }
        writeWholeFile("bi.json",
            "{\n"
            "  \"name\": \"myapp\",\n"
            "  \"version\": \"0.1.0\",\n"
            "  \"entry\": \"src/main.bi\",\n"
            "  \"dependencies\": {}\n"
            "}\n");

        mkdir("src", 0755);
        writeWholeFile("src/main.bi",
            "// myapp — a .bi web app\n\n"
            "route GET \"/\" {\n"
            "  return \"<h1>Hello from .bi</h1>\"\n"
            "}\n\n"
            "route GET \"/api/hello\" {\n"
            "  return json({ message: \"hello\", time: time() })\n"
            "}\n\n"
            "serve(8080)\n");

        std::cout << "bi: created bi.json and src/main.bi\n";
        return 0;
    }

    // ---- bi new <name> ----
    static int newProject(const std::string& name) {
        if (fileExists(name)) {
            std::cerr << "bi: directory '" << name << "' already exists\n";
            return 1;
        }
        mkdir(name.c_str(), 0755);
        std::string src = name + "/src";
        mkdir(src.c_str(), 0755);

        writeWholeFile(name + "/bi.json",
            "{\n"
            "  \"name\": \"" + name + "\",\n"
            "  \"version\": \"0.1.0\",\n"
            "  \"entry\": \"src/main.bi\",\n"
            "  \"dependencies\": {}\n"
            "}\n");
        writeWholeFile(src + "/main.bi",
            "route GET \"/\" {\n"
            "  return \"<h1>" + name + " running on .bi</h1>\"\n"
            "}\n\n"
            "serve(8080)\n");

        std::cout << "bi: created project '" << name << "'\n";
        std::cout << "    cd " << name << " && bi serve src/main.bi\n";
        return 0;
    }

    // ---- bi install <name> ----
    static int install(const std::string& name) {
        std::string reg = defaultRegistry();
        mkdir("bi_modules", 0755);

        std::string url  = reg + "/" + name + ".tar.gz";
        std::string tmp  = "/tmp/bi-" + name + ".tar.gz";
        std::string dest = "bi_modules/" + name;

        std::cout << "bi: fetching " << url << "\n";
        std::string cmd = "curl -sSL --fail --max-time 300 -o '" + tmp + "' '" + url + "' 2>/dev/null";
        if (system(cmd.c_str()) != 0 || !fileExists(tmp)) {
            std::cerr << "bi: failed to download package '" << name << "'\n";
            std::cerr << "    registry: " << reg << "\n";
            return 1;
        }

        mkdir(dest.c_str(), 0755);
        std::string untar = "tar -xzf '" + tmp + "' -C '" + dest + "' 2>/dev/null";
        if (system(untar.c_str()) != 0) {
            std::cerr << "bi: failed to extract package '" << name << "'\n";
            return 1;
        }

        addDependency(name);
        std::cout << "bi: installed " << name << " -> " << dest << "\n";
        return 0;
    }

    // ---- bi install  (all deps) ----
    static int installAll() {
        if (!fileExists("bi.json")) {
            std::cerr << "bi: no bi.json found — run 'bi init' first\n";
            return 1;
        }
        Value manifest;
        try {
            manifest = parseJson(readWholeFile("bi.json"));
        } catch (std::exception& e) {
            std::cerr << "bi: invalid bi.json: " << e.what() << "\n";
            return 1;
        }
        if (manifest.type != Value::MAP) {
            std::cerr << "bi: bi.json must be an object\n";
            return 1;
        }
        auto it = manifest.map->find("dependencies");
        if (it == manifest.map->end() || it->second.type != Value::MAP) {
            std::cout << "bi: no dependencies\n";
            return 0;
        }
        int rc = 0;
        for (auto& kv : *it->second.map) {
            if (install(kv.first) != 0) rc = 1;
        }
        return rc;
    }

    // ---- bi remove <name> ----
    static int remove(const std::string& name) {
        std::string dir = "bi_modules/" + name;
        if (!fileExists(dir)) {
            std::cerr << "bi: package '" << name << "' is not installed\n";
            return 1;
        }
        std::string cmd = "rm -rf '" + dir + "'";
        if (system(cmd.c_str()) != 0) return 1;

        if (fileExists("bi.json")) {
            try {
                Value m = parseJson(readWholeFile("bi.json"));
                if (m.type == Value::MAP) {
                    auto it = m.map->find("dependencies");
                    if (it != m.map->end() && it->second.type == Value::MAP) {
                        it->second.map->erase(name);
                        writeWholeFile("bi.json", toJson(m));
                    }
                }
            } catch (...) {}
        }
        std::cout << "bi: removed " << name << "\n";
        return 0;
    }

    // ---- bi list ----
    static int list() {
        if (!fileExists("bi_modules")) {
            std::cout << "bi: no packages installed\n";
            return 0;
        }
        std::string out = shellCapture("ls -1 bi_modules 2>/dev/null");
        if (out.empty()) { std::cout << "bi: no packages installed\n"; return 0; }
        std::cout << "Installed packages:\n";
        std::istringstream ss(out);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.empty()) continue;
            std::string ver = "?";
            std::string mf = "bi_modules/" + line + "/bi.json";
            if (fileExists(mf)) {
                try {
                    Value m = parseJson(readWholeFile(mf));
                    if (m.type == Value::MAP) {
                        auto it = m.map->find("version");
                        if (it != m.map->end() && it->second.type == Value::STR) ver = it->second.str;
                    }
                } catch (...) {}
            }
            std::cout << "  " << line << "  " << ver << "\n";
        }
        return 0;
    }

private:
    static void addDependency(const std::string& name) {
        if (!fileExists("bi.json")) return;
        try {
            Value m = parseJson(readWholeFile("bi.json"));
            if (m.type != Value::MAP) return;
            auto it = m.map->find("dependencies");
            if (it == m.map->end() || it->second.type != Value::MAP) {
                (*m.map)["dependencies"] = vmap(std::make_shared<ValueMap>());
                it = m.map->find("dependencies");
            }
            (*it->second.map)[name] = vstr("*");
            writeWholeFile("bi.json", toJson(m));
        } catch (...) {}
    }
};

} // namespace bi