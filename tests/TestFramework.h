#pragma once

#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace test {
namespace terminal {
inline bool colorsEnabled() {
  // Respect the conventional opt-out and common terminal color controls.
  if (const char *noColor = std::getenv("NO_COLOR");
      noColor != nullptr && *noColor != '\0') {
    return false;
  }
  if (const char *forceColor = std::getenv("CLICOLOR_FORCE");
      forceColor != nullptr && std::string(forceColor) != "0") {
    return true;
  }
  if (const char *color = std::getenv("CLICOLOR");
      color != nullptr && std::string(color) == "0") {
    return false;
  }
  if (const char *term = std::getenv("TERM");
      term != nullptr && std::string(term) == "dumb") {
    return false;
  }
  return true;
}

inline std::string paint(const std::string &text, const char *colorCode) {
  if (!colorsEnabled()) return text;
  return std::string(colorCode) + text + "\033[0m";
}

inline constexpr const char *green = "\033[1;32m";
inline constexpr const char *red = "\033[1;31m";
}  // namespace terminal

struct Case {
  std::string name;
  void (*run)();
};

inline std::vector<Case> &registry() {
  static std::vector<Case> cases;
  return cases;
}

class Registrar {
 public:
  Registrar(std::string name, void (*run)()) {
    registry().push_back({std::move(name), run});
  }
};

inline void expect(bool condition, const char *expression,
                   const char *file, int line) {
  if (condition) return;
  std::ostringstream message;
  message << file << ':' << line << ": expectation failed: " << expression;
  throw std::runtime_error(message.str());
}

template <typename Expected, typename Actual>
void expectEqual(const Expected &expected, const Actual &actual,
                 const char *expression, const char *file, int line) {
  if (expected == actual) return;
  std::ostringstream message;
  message << file << ':' << line << ": expected equality failed: " << expression;
  throw std::runtime_error(message.str());
}

inline int runAll() {
  int passed = 0;
  int failed = 0;
  for (const auto &testCase : registry()) {
    try {
      testCase.run();
      ++passed;
      std::cout << terminal::paint("[PASS] " + testCase.name, terminal::green)
                << '\n';
    } catch (const std::exception &error) {
      ++failed;
      std::cerr << terminal::paint("[FAIL] " + testCase.name + ": " + error.what(),
                                   terminal::red)
                << '\n';
    } catch (...) {
      ++failed;
      std::cerr << terminal::paint("[FAIL] " + testCase.name + ": unknown exception",
                                   terminal::red)
                << '\n';
    }
  }

  std::ostringstream summary;
  summary << "Test summary: " << passed << " passed, " << failed << " failed, "
          << registry().size() << " total";
  const char *summaryColor = failed == 0 ? terminal::green : terminal::red;
  std::cout << '\n' << terminal::paint(summary.str(), summaryColor) << '\n';

  return failed == 0 ? 0 : 1;
}
}  // namespace test

#define TEST_CASE(name)                                          \
  static void name();                                            \
  static const ::test::Registrar name##_registrar(#name, &name); \
  static void name()
#define EXPECT_TRUE(expression) \
  ::test::expect(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
#define EXPECT_FALSE(expression) EXPECT_TRUE(!(expression))
#define EXPECT_EQ(expected, actual) ::test::expectEqual((expected), (actual), #expected " == " #actual, __FILE__, __LINE__)