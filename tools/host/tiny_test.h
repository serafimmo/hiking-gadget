#pragma once
// Minimal test framework (no external dependency): TEST, CHECK, CHECK_EQ, CHECK_NEAR, CHECK_STR.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <type_traits>

namespace tt {
struct Case { const char* name; const char* file; void (*fn)(); Case* next; };
inline Case*& head() { static Case* h = nullptr; return h; }
inline Case*& tail() { static Case* t = nullptr; return t; }
inline int& failures() { static int f = 0; return f; }
inline long& checks() { static long c = 0; return c; }
inline const char*& current() { static const char* c = ""; return c; }
struct Reg { explicit Reg(Case* c) { (head() ? tail()->next : head()) = c; tail() = c; } };

template <typename T> void show(char* buf, size_t n, const T& v) {
  if constexpr (std::is_enum_v<T>) snprintf(buf, n, "%lld", (long long)v);
  else if constexpr (std::is_same_v<T, bool>) snprintf(buf, n, "%s", v ? "true" : "false");
  else if constexpr (std::is_integral_v<T>) snprintf(buf, n, "%lld", (long long)v);
  else if constexpr (std::is_floating_point_v<T>) snprintf(buf, n, "%.9g", (double)v);
  else if constexpr (std::is_convertible_v<T, const char*>) snprintf(buf, n, "\"%s\"", (const char*)v);
  else snprintf(buf, n, "?");
}
inline void fail(const char* file, int line, const char* expr, const char* a = nullptr, const char* b = nullptr) {
  ++failures();
  printf("FAIL [%s] %s:%d: %s", current(), file, line, expr);
  if (a) printf("  (%s vs %s)", a, b);
  printf("\n");
}
template <typename A, typename B> void eq(const A& a, const B& b, const char* file, int line, const char* expr) {
  ++checks();
  if (!(a == b)) { char x[64], y[64]; show(x, sizeof x, a); show(y, sizeof y, b); fail(file, line, expr, x, y); }
}
inline void near(double a, double b, double eps, const char* file, int line, const char* expr) {
  ++checks();
  if (!(std::fabs(a - b) <= eps)) { char x[64], y[64]; show(x, sizeof x, a); show(y, sizeof y, b); fail(file, line, expr, x, y); }
}
inline void streq(const char* a, const char* b, const char* file, int line, const char* expr) {
  ++checks();
  if (!a || !b || strcmp(a, b) != 0) fail(file, line, expr, a ? a : "(null)", b ? b : "(null)");
}
inline int runAll(const char* filter) {
  int ran = 0;
  for (Case* c = head(); c; c = c->next) {
    if (filter && !strstr(c->name, filter) && !strstr(c->file, filter)) continue;
    current() = c->name; ++ran;
    c->fn();
  }
  printf("%s: %d tests, %ld checks, %d failures%s%s\n", failures() ? "FAILED" : ran ? "OK" : "NO TESTS RAN", ran, checks(), failures(),
         filter ? ", filter " : "", filter ? filter : "");
  return failures() || ran == 0 ? 1 : 0;
}
}  // namespace tt

#define TEST(name)                                                       \
  static void name();                                                    \
  static tt::Case name##_case{#name, __FILE__, name, nullptr};           \
  static tt::Reg name##_reg(&name##_case);                               \
  static void name()
#define CHECK(c) do { ++tt::checks(); if (!(c)) tt::fail(__FILE__, __LINE__, #c); } while (0)
#define CHECK_EQ(a, b) tt::eq((a), (b), __FILE__, __LINE__, #a " == " #b)
#define CHECK_NEAR(a, b, eps) tt::near((a), (b), (eps), __FILE__, __LINE__, #a " ~ " #b)
#define CHECK_STR(a, b) tt::streq((a), (b), __FILE__, __LINE__, #a " == " #b)
