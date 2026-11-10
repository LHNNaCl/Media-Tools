#pragma once

#include <exception>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

struct TestCase {
    const char* name;
    void (*function)();
};

inline std::vector<TestCase>& testCases() {
    static std::vector<TestCase> all;
    return all;
}

inline int& testFailures() {
    static int count = 0;
    return count;
}

struct TestRegistration {
    TestRegistration(const char* name, void (*function)()) { testCases().push_back({name, function}); }
};

#define TEST(name)                                                                 \
    void name();                                                                   \
    static TestRegistration registration_##name(#name, &name);                     \
    void name()

inline void expectCondition(bool condition, const char* text, int line) {
    if (!condition) {
        std::cerr << "line " << line << ": " << text << '\n';
        ++testFailures();
    }
}

#define EXPECT(condition) expectCondition(static_cast<bool>(condition), #condition, __LINE__)
