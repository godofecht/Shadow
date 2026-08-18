// Test framework header
#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cmath>
#include <stdexcept>

// Test framework
class TestRunner
{
public:
    using TestFunction = std::function<void()>;
    
    static TestRunner& getInstance()
    {
        static TestRunner instance;
        return instance;
    }
    
    void registerTest(const std::string& name, TestFunction func)
    {
        // Guard against duplicate test names across translation units:
        // a silently-dropped or double-run test is worse than a loud failure.
        for (const auto& test : tests)
        {
            if (test.name == name)
            {
                std::cerr << "FATAL: Duplicate test name registered: " << name << std::endl;
                std::exit(1);
            }
        }
        tests.push_back({name, func});
    }
    
    // Run every registered test, or only those whose name contains `filter`
    // (the unit binary accepts it as argv[1] so focused CI/valgrind passes
    // can run one code path without the whole suite).
    int runAllTests(const std::string& filter = "")
    {
        int passed = 0;
        int failed = 0;
        int skipped = 0;
        
        std::cout << "=================================" << std::endl;
        if (!filter.empty()) {
            std::cout << "Running tests matching '" << filter << "'..." << std::endl;
        } else {
            std::cout << "Running " << tests.size() << " tests..." << std::endl;
        }
        std::cout << "=================================" << std::endl;
        
        for (const auto& test : tests)
        {
            if (!filter.empty() && test.name.find(filter) == std::string::npos) {
                skipped++;
                continue;
            }
            std::cout << "Test: " << test.name << "... ";
            try
            {
                test.func();
                std::cout << "PASSED" << std::endl;
                passed++;
            }
            catch (const std::exception& e)
            {
                std::cout << "FAILED: " << e.what() << std::endl;
                failed++;
            }
            catch (...)
            {
                std::cout << "FAILED: Unknown exception" << std::endl;
                failed++;
            }
        }
        
        std::cout << "=================================" << std::endl;
        std::cout << "Results: " << passed << " passed, " << failed << " failed";
        if (skipped > 0) std::cout << ", " << skipped << " skipped";
        std::cout << std::endl;
        std::cout << "=================================" << std::endl;
        
        // A filter that matches nothing must fail loudly rather than pass
        // with zero coverage (a typo'd focused CI entry would otherwise
        // silently go green).
        if (!filter.empty() && passed == 0 && failed == 0) {
            std::cout << "No tests matched filter '" << filter << "'" << std::endl;
            return 1;
        }
        return failed == 0 ? 0 : 1;
    }
    
private:
    struct Test
    {
        std::string name;
        TestFunction func;
    };
    
    std::vector<Test> tests;
};

// Test registration macro
#define REGISTER_TEST(name) \
    void name(); \
    struct name##Register { \
        name##Register() { \
            TestRunner::getInstance().registerTest(#name, name); \
        } \
    } name##Instance; \
    void name()

// Assertion macros - all failures include file:line for CI debugging
#define ASSERT_TRUE(expr) \
    if (!(expr)) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + \
        " ASSERT_TRUE failed: " #expr)

#define ASSERT_FALSE(expr) \
    if (expr) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + \
        " ASSERT_FALSE failed: " #expr)

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + \
        " ASSERT_EQ failed: " #a " != " #b)

#define ASSERT_NEAR(a, b, eps) \
    if (std::abs((a) - (b)) > (eps)) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + \
        " ASSERT_NEAR failed: " #a " and " #b " differ by more than " #eps)
