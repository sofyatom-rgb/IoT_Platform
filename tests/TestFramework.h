#pragma once

#include <exception>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace test {

/// Тест и его функция запуска.
struct Case {
  std::string name;
  void (*run)();
};

/// Возвращает общий список зарегистрированных тестов.
inline std::vector<Case> &registry() {
  static std::vector<Case> cases;
  return cases;
}

/// Добавляет тест в список запуска.
class Registrar {
 public:
  /// Регистрирует тест при загрузке файла.
  Registrar(std::string name, void (*run)()) {
    registry().push_back({std::move(name), run});
  }
};

/// Проверяет, можно ли безопасно использовать ANSI-цвета для потока.
inline bool colorEnabled(FILE *stream) {
  const char *noColor = std::getenv("NO_COLOR");
  if (noColor != nullptr && *noColor != '\0') return false;

  const auto enabled = [](const char *name) {
    const char *value = std::getenv(name);
    return value != nullptr && *value != '\0' && std::string(value) != "0";
  };
  if (enabled("FORCE_COLOR") || enabled("CLICOLOR_FORCE")) return true;

  const char *term = std::getenv("TERM");
  if (term != nullptr && std::string(term) == "dumb") return false;

#ifdef _WIN32
  return _isatty(_fileno(stream)) != 0;
#else
  return isatty(fileno(stream)) != 0;
#endif
}

/// Добавляет ANSI-код, если вывод поддерживает цвета.
inline std::string paint(const std::string &text, const char *code,
                         bool enabled) {
  if (!enabled) return text;
  return std::string(code) + text + "\033[0m";
}

/// Завершает тест с ошибкой, если условие не выполнено.
inline void expect(bool condition, const char *expression,
                   const char *file, int line) {
  if (condition) return;
  std::ostringstream message;
  message << file << ':' << line << ": expectation failed: " << expression;
  throw std::runtime_error(message.str());
}

/// Проверяет равенство двух значений.
template <typename Expected, typename Actual>
void expectEqual(const Expected &expected, const Actual &actual,
                 const char *expression, const char *file, int line) {
  if (expected == actual) return;
  std::ostringstream message;
  message << file << ':' << line << ": expected equality failed: " << expression;
  throw std::runtime_error(message.str());
}

/// Запускает зарегистрированные тесты и возвращает код результата.
inline int runAll() {
  constexpr const char *green = "\033[32m";
  constexpr const char *red = "\033[31m";

  const bool colorOut = colorEnabled(stdout);
  const bool colorErr = colorEnabled(stderr);
  int passed = 0;
  int failed = 0;

  for (const auto &testCase : registry()) {
    try {
      testCase.run();
      ++passed;
      std::cout << paint("[PASS] " + testCase.name, green, colorOut) << '\n';
    } catch (const std::exception &error) {
      ++failed;
      std::cerr << paint("[FAIL] " + testCase.name + ": " + error.what(),
                         red, colorErr)
                << '\n';
    } catch (...) {
      ++failed;
      std::cerr << paint("[FAIL] " + testCase.name + ": unknown exception",
                         red, colorErr)
                << '\n';
    }
  }

  std::cout << "\nTest summary: "
            << paint(std::to_string(passed) + " passed", green, colorOut)
            << ", "
            << paint(std::to_string(failed) + " failed",
                     failed == 0 ? green : red, colorOut)
            << ", " << registry().size() << " total\n";
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
#define EXPECT_EQ(expected, actual) \
  ::test::expectEqual((expected), (actual), #expected " == " #actual, __FILE__, __LINE__)
