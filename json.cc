#include "json.h"

#include <cassert>
#include <cstddef>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "parse.h"
#include "unicode.h"

namespace json {

namespace {
// Overloaded helper from
// https://en.cppreference.com/w/cpp/utility/variant/visit#Example
template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;
}  // namespace

std::optional<Value> Parse(std::string_view s) {
  s = internal::SkipWS(s);
  auto opt = internal::ParseValue(s);
  if (!opt) return std::nullopt;
  s = opt->second;
  s = internal::SkipWS(s);
  if (!s.empty()) return std::nullopt;
  return {std::move(opt->first)};
}

Value Clone(const Value& v) {
  return Visit(Overloaded{
                   [](std::nullptr_t) { return Value(nullptr); },
                   [](bool bv) { return Value(bv); },
                   [](double nv) { return Value(nv); },
                   [](const std::string& sv) { return Value(sv); },
                   [](const Object& ov) {
                     Object out;
                     out.reserve(ov.size());
                     for (const auto& [key, value] : ov) {
                       out.insert_or_assign(
                           key, std::make_unique<Value>(Clone(*value)));
                     }
                     return Value(std::move(out));
                   },
                   [](const Array& av) {
                     Array out;
                     out.reserve(av.size());
                     for (const auto& el : av) {
                       out.push_back(std::make_unique<Value>(Clone(*el)));
                     }
                     return Value(std::move(out));
                   },
               },
               v);
}

namespace {

struct Hex4 {
  char16_t n;
};

constexpr char ToHexDigit(char n) {
  if (0 <= n && n <= 9) return n - 0x0 + '0';
  if (0xa <= n && n <= 0xf) return n - 0xa + 'a';
  assert(false);
}

std::ostream& operator<<(std::ostream& os, Hex4 cu) {
  return os << ToHexDigit(cu.n / 0x1000) << ToHexDigit(cu.n / 0x100 % 0x10)
            << ToHexDigit(cu.n / 0x10 % 0x10) << ToHexDigit(cu.n % 0x10);
}

struct JSONString {
  std::string_view sv;

  template <typename... T>
  JSONString(T&&... t) : sv(std::forward<T>(t)...) {}
};

std::ostream& operator<<(std::ostream& os, JSONString str) {
  os << '"';
  char buf[U8_MAX_LENGTH];
  while (!str.sv.empty()) {
    char32_t cp;
    std::tie(cp, str.sv) = internal::utf8::Decode(str.sv);
    if (cp == '\b')
      os << "\\b";
    else if (cp == '\t')
      os << "\\t";
    else if (cp == '\n')
      os << "\\n";
    else if (cp == '\f')
      os << "\\f";
    else if (cp == '\r')
      os << "\\r";
    else if (cp == '"')
      os << "\\\"";
    else if (cp == '\\')
      os << "\\\\";
    else if (cp < 0x20)
      os << "\\u" << Hex4{static_cast<char16_t>(cp)};
    else
      os << internal::utf8::Encode(cp, buf);
  }
  os << '"';
  return os;
}

std::ostream& operator<<(std::ostream& os, const Value& v) {
  Visit(Overloaded{
            [&os](std::nullptr_t) { os << "null"; },
            [&os](bool bv) { os << (bv ? "true" : "false"); },
            [&os](double nv) { os << nv; },
            [&os](const std::string& sv) { os << JSONString(sv); },
            [&os](const Object& ov) {
              os << '{';
              auto it = ov.begin();
              if (it != ov.end()) {
                os << JSONString(it->first) << ':' << *it->second;
                ++it;
              }
              for (; it != ov.end(); ++it) {
                os << ',';
                os << JSONString(it->first) << ':' << *it->second;
              }
              os << '}';
            },
            [&os](const Array& av) {
              os << '[';
              auto it = av.begin();
              if (it != av.end()) {
                os << **it;
                ++it;
              }
              for (; it != av.end(); ++it) {
                os << ',';
                os << **it;
              }
              os << ']';
            },
        },
        v);
  return os;
}

}  // namespace

void Print(const Value& v) { std::cout << v << '\n'; }

}  // namespace json
