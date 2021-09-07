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

constexpr char ToHexDigit(char n) {
  if (0 <= n && n <= 9) return n - 0x0 + '0';
  if (0xa <= n && n <= 0xf) return n - 0xa + 'a';
  assert(false);
}

void AppendHex4(std::string* s, char16_t n) {
  s->push_back(ToHexDigit(n / 0x1000));
  s->push_back(ToHexDigit(n / 0x100 % 0x10));
  s->push_back(ToHexDigit(n / 0x10 % 0x10));
  s->push_back(ToHexDigit(n % 0x10));
}

}  // namespace

void Append(std::string* s, double n) {
  // FIXME: Maybe use absl::StrAppend or something similar.
  *s += std::to_string(n);
}

void Append(std::string* s, const std::string& t) {
  s->push_back('"');
  char buf[U8_MAX_LENGTH];
  std::string_view tv = t;
  while (!tv.empty()) {
    char32_t cp;
    std::tie(cp, tv) = internal::utf8::Decode(tv);
    if (cp == '\b')
      s->append("\\b");
    else if (cp == '\t')
      s->append("\\t");
    else if (cp == '\n')
      s->append("\\n");
    else if (cp == '\f')
      s->append("\\f");
    else if (cp == '\r')
      s->append("\\r");
    else if (cp == '"')
      s->append("\\\"");
    else if (cp == '\\')
      s->append("\\\\");
    else if (cp < 0x20) {
      s->append("\\u");
      AppendHex4(s, static_cast<char16_t>(cp));
    } else
      s->append(internal::utf8::Encode(cp, buf));
  }
  s->push_back('"');
}

void Append(std::string* s, const Object& ov) {
  s->push_back('{');
  auto it = ov.begin();
  if (it != ov.end()) {
    Append(s, it->first);
    s->push_back(':');
    Append(s, *it->second);
    ++it;
  }
  for (; it != ov.end(); ++it) {
    s->push_back(',');
    Append(s, it->first);
    s->push_back(':');
    Append(s, *it->second);
  }
  s->push_back('}');
}

void Append(std::string* s, const Array& av) {
  s->push_back('[');
  auto it = av.begin();
  if (it != av.end()) {
    Append(s, **it);
    ++it;
  }
  for (; it != av.end(); ++it) {
    s->push_back(',');
    Append(s, **it);
  }
  s->push_back(']');
}

void Append(std::string* s, const Value& v) {
  Visit([s](const auto& inner) { Append(s, inner); }, v);
}

void Print(const Value& v) {
  std::string s;
  Append(&s, v);
  std::cout << s << '\n';
}

}  // namespace json
