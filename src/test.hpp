#pragma once
#include "interpreter.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace bi {

inline int runTests(const std::string& dir) {
    namespace fs = std::filesystem;

    if (!fs::exists(dir)) {
        std::cerr << "bi test: no '" << dir << "' directory\n";
        return 1;
    }

    std::vector<fs::path> files;
    for (auto& e : fs::directory_iterator(dir)) {
        if (e.is_regular_file() && e.path().extension() == ".bi")
            files.push_back(e.path());
    }
    std::sort(files.begin(), files.end());

    if (files.empty()) {
        std::cerr << "bi test: no *.bi files in '" << dir << "'\n";
        return 1;
    }

    int passed = 0, failed = 0;

    for (auto& f : files) {
        Interpreter interp;
        std::string name = f.filename().string();

        try {
            interp.runFile(f.string());
            std::cout << "  \xE2\x9C\x93 " << name << "\n";
            passed++;
        } catch (BiError& e) {
            std::cout << "  \xE2\x9C\x97 " << name << "\n"
                      << "      " << e.what();
            if (e.line > 0) std::cout << " (line " << e.line << ")";
            std::cout << "\n";
            failed++;
        } catch (ThrowSignal& t) {
            std::cout << "  \xE2\x9C\x97 " << name
                      << "\n      unhandled throw: " << toStr(t.value) << "\n";
            failed++;
        } catch (std::exception& e) {
            std::cout << "  \xE2\x9C\x97 " << name << "\n      " << e.what() << "\n";
            failed++;
        }
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}

} // namespace bi