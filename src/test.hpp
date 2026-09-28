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

    int filePassed = 0, fileFailed = 0;
    int assertionPassed = 0, assertionFailed = 0;

    for (auto& f : files) {
        auto& ts = testState();
        ts.describeStack.clear();
        ts.passed = 0;
        ts.failed = 0;

        Interpreter interp;
        std::string name = f.filename().string();

        std::cout << "\n  \xE2\x96\xB8 " << name << "\n";

        bool fileOk = true;
        try {
            interp.runFile(f.string());
        } catch (BiError& e) {
            fileOk = false;
            std::cout << "    \xE2\x9C\x97 crashed: " << e.what();
            if (e.line > 0) std::cout << " (line " << e.line << ")";
            std::cout << "\n";
            for (auto& fr : e.trace) {
                std::cout << "        at " << fr.function
                          << " (" << fr.file << ":" << fr.line << ")\n";
            }
        } catch (ThrowSignal& t) {
            fileOk = false;
            std::cout << "    \xE2\x9C\x97 unhandled throw: " << toStr(t.value) << "\n";
        } catch (std::exception& e) {
            fileOk = false;
            std::cout << "    \xE2\x9C\x97 " << e.what() << "\n";
        }

        assertionPassed += ts.passed;
        assertionFailed += ts.failed;

        // If the file didn't register any it() blocks, treat pass/fail as file-level.
        if (ts.passed == 0 && ts.failed == 0) {
            if (fileOk) {
                std::cout << "    \xE2\x9C\x93 passed\n";
                filePassed++;
            } else {
                fileFailed++;
            }
        } else {
            if (ts.failed == 0 && fileOk) filePassed++;
            else                          fileFailed++;
        }
    }

    std::cout << "\n";
    if (assertionPassed + assertionFailed > 0) {
        std::cout << assertionPassed << " assertions passed, "
                  << assertionFailed << " failed\n";
    }
    std::cout << filePassed << " files passed, " << fileFailed << " files failed\n";
    return fileFailed == 0 ? 0 : 1;
}

} // namespace bi