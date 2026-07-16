#pragma once

#include <cstdint>
#include <exception>
#include <iostream>
#include <string_view>

namespace truth::runtime::tests {

class Failure final : public std::exception {
public:
    explicit Failure(const std::string_view message) noexcept:
        message_(message)
    {}

    [[nodiscard]] const char* what() const noexcept override
    {
        return message_.data();
    }

private:
    std::string_view message_;
};

struct Context final {
    std::uint64_t assertions{0};

    void expect(const bool condition, const std::string_view message)
    {
        ++assertions;
        if (!condition) {
            throw Failure{message};
        }
    }
};

using TestFunction = void (*)(Context&);

struct TestCase final {
    std::string_view name;
    TestFunction function;
};

inline int Run(
    const std::string_view suite,
    const TestCase* tests,
    const std::size_t count)
{
    Context context;
    std::size_t passed = 0;
    for (std::size_t index = 0; index < count; ++index) {
        try {
            tests[index].function(context);
            ++passed;
            std::cout << "[PASS] " << tests[index].name << '\n';
        } catch (const std::exception& exception) {
            std::cerr << "[FAIL] " << tests[index].name << ": "
                      << exception.what() << '\n';
            return 1;
        }
    }
    std::cout << suite << ": " << passed << '/' << count
              << "; assertions: " << context.assertions << '\n';
    return 0;
}

} // namespace truth::runtime::tests
