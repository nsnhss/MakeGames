#pragma once
// 最小自包含测试框架：零第三方依赖，保证离线可构建。
// 用法：
//   MG_TEST(my_test) { MG_EXPECT(1 + 1 == 2); MG_EXPECT_EQ(a, b); }
//   在恰好一个 cpp 中放 MG_TEST_MAIN()。

#include <cstdio>
#include <string>
#include <vector>

namespace mgtest {

struct TestCase {
    const char* name;
    void (*fn)();
};

inline std::vector<TestCase>& Registry() { static std::vector<TestCase> r; return r; }
inline int& FailureCount() { static int n = 0; return n; }
inline int& CheckCount() { static int n = 0; return n; }

struct Registrar {
    Registrar(const char* name, void (*fn)()) { Registry().push_back({ name, fn }); }
};

inline void ReportFailure(const char* file, int line, const std::string& expr) {
    ++FailureCount();
    std::printf("  [FAIL] %s:%d: %s\n", file, line, expr.c_str());
}

} // namespace mgtest

#define MG_TEST(name) \
    static void name(); \
    static ::mgtest::Registrar registrar_##name(#name, &name); \
    static void name()

#define MG_EXPECT(cond) do { \
    ++::mgtest::CheckCount(); \
    if (!(cond)) ::mgtest::ReportFailure(__FILE__, __LINE__, "EXPECT(" #cond ")"); \
} while (0)

#define MG_EXPECT_EQ(a, b) do { \
    ++::mgtest::CheckCount(); \
    if (!((a) == (b))) ::mgtest::ReportFailure(__FILE__, __LINE__, "EXPECT_EQ(" #a ", " #b ")"); \
} while (0)

#define MG_TEST_MAIN() \
int main() { \
    int failed_tests = 0; \
    for (auto& tc : ::mgtest::Registry()) { \
        int before = ::mgtest::FailureCount(); \
        std::printf("[ RUN  ] %s\n", tc.name); \
        tc.fn(); \
        if (::mgtest::FailureCount() != before) { ++failed_tests; std::printf("[FAILED] %s\n", tc.name); } \
        else std::printf("[  OK  ] %s\n", tc.name); \
    } \
    std::printf("\n%d checks, %d failures, %d/%d tests failed\n", \
        ::mgtest::CheckCount(), ::mgtest::FailureCount(), failed_tests, (int)::mgtest::Registry().size()); \
    return failed_tests == 0 ? 0 : 1; \
}
