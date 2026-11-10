#include "test_harness.hpp"

#include <iostream>

int main() {
    int failed = 0;
    for (const auto& test : testCases()) {
        testFailures() = 0;
        try {
            test.function();
        } catch (const std::exception& ex) {
            std::cerr << test.name << " threw " << ex.what() << '\n';
            ++testFailures();
        } catch (...) {
            std::cerr << test.name << " threw\n";
            ++testFailures();
        }
        if (testFailures()) {
            std::cerr << "FAIL " << test.name << '\n';
            ++failed;
        } else {
            std::cout << "OK " << test.name << '\n';
        }
    }
    const auto passed = static_cast<int>(testCases().size()) - failed;
    std::cout << (failed ? "FAILED " : "PASSED ") << passed << "/" << testCases().size() << '\n';
    return failed ? 1 : 0;
}
