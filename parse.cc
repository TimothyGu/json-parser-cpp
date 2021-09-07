#include "parse.h"

#include <cassert>
#include <charconv>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "unicode.h"

namespace json {
namespace internal {

namespace {
using std::literals::string_view_literals::operator""sv;

inline bool IsWS(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

std::optional<uint8_t> ParseHex(char c) {
  if ('0' <= c && c <= '9') return c - '0' + 0x0;
  if ('A' <= c && c <= 'F') return c - 'A' + 0xA;
  if ('a' <= c && c <= 'f') return c - 'a' + 0xa;
  return std::nullopt;
}

std::optional<char16_t> Parse4Hex(std::string_view s) {
  if (s.size() < 4) return std::nullopt;

  char16_t out = 0;
  if (auto opt = ParseHex(s[0])) {
    out += opt.value() * 0x1000;
    if (auto opt = ParseHex(s[1])) {
      out += opt.value() * 0x100;
      if (auto opt = ParseHex(s[2])) {
        out += opt.value() * 0x10;
        if (auto opt = ParseHex(s[3])) {
          out += opt.value();
          return out;
        }
      }
    }
  }
  return std::nullopt;
}

constexpr std::string_view kInvalidUTF8 = "\xEF\xBF\xBD"sv;  // U+FFFD
}  // namespace

std::string_view SkipWS(std::string_view s) {
  while (!s.empty()) {
    if (IsWS(s[0])) {
      s.remove_prefix(1);
      continue;
    }
    break;
  }
  return s;
}

std::optional<std::pair<std::string, std::string_view>> ParseString(
    std::string_view s) {
  if (s.empty() || s[0] != '"') return std::nullopt;
  s.remove_prefix(1);

  std::string out;
  constexpr char32_t kNoPending = -1;
  char32_t pending = kNoPending;

  while (!s.empty()) {
    if (s[0] == '"') {
      s.remove_prefix(1);
      if (pending != kNoPending) out.append(kInvalidUTF8);
      return {{std::move(out), s}};
    } else if (s[0] == '\\') {
      s.remove_prefix(1);
      if (s.empty()) return std::nullopt;

      char converted;
      switch (s[0]) {
        case '"':
        case '\\':
        case '/':
          converted = s[0];
          break;
        case 'b':
          converted = '\b';
          break;
        case 'f':
          converted = '\f';
          break;
        case 'n':
          converted = '\n';
          break;
        case 'r':
          converted = '\r';
          break;
        case 't':
          converted = '\t';
          break;
        case 'u':
          s.remove_prefix(1);
          if (auto value = Parse4Hex(s)) {
            s.remove_prefix(4);
            char16_t cu = value.value();
            if (utf16::IsLeadSurrogate(cu)) {
              if (pending != kNoPending) out.append(kInvalidUTF8);
              pending = cu;
            } else if (utf16::IsTrailingSurrogate(cu)) {
              if (pending == kNoPending)
                out.append(kInvalidUTF8);
              else {
                char buf[U8_MAX_LENGTH];
                out.append(utf8::Encode(utf16::Compose(pending, cu), buf));
                pending = kNoPending;
              }
            } else {
              if (pending != kNoPending) {
                out.append(kInvalidUTF8);
                pending = kNoPending;
              }
              char buf[U8_MAX_LENGTH];
              out.append(utf8::Encode(cu, buf));
            }
            continue;
          } else {
            return std::nullopt;
          }
        default:
          return std::nullopt;
      }

      if (pending != kNoPending) {
        out.append(kInvalidUTF8);
        pending = kNoPending;
      }
      out.push_back(converted);
      s.remove_prefix(1);
    } else if (static_cast<char8_t>(s[0]) < 0x20) {
      return std::nullopt;
    } else if (static_cast<char8_t>(s[0]) < 0x80) {
      if (pending != kNoPending) {
        out.append(kInvalidUTF8);
        pending = kNoPending;
      }
      out.push_back(s[0]);
      s.remove_prefix(1);
    } else {
      // Surrogates cannot be encoded in UTF-8. If we have a pending surrogate,
      // then it must be unpaired.
      if (pending != kNoPending) {
        out.append(kInvalidUTF8);
        pending = kNoPending;
      }

      // UTF-8 roundtrip
      char32_t cp;
      std::tie(cp, s) = utf8::Decode(s);
      char buf[U8_MAX_LENGTH];
      out.append(utf8::Encode(cp, buf));
    }
  }

  return std::nullopt;
}

std::optional<std::pair<double, std::string_view>> ParseNumber(
    std::string_view s) {
  std::string_view orig = s;
  if (s.empty()) return std::nullopt;
  if (s[0] == '-') s.remove_prefix(1);
  if (s.empty()) return std::nullopt;

  // Consume integer part.
  if (s[0] == '0') {
    s.remove_prefix(1);
  } else if ('1' <= s[0] && s[0] <= '9') {
    s.remove_prefix(1);
    while (!s.empty()) {
      if ('0' <= s[0] && s[0] <= '9')
        s.remove_prefix(1);
      else
        break;
    }
  } else {
    return std::nullopt;
  }

  // Consume optional fractional part lazily.
  {
    std::string_view frac = s;
    if (!frac.empty() && frac[0] == '.') {
      frac.remove_prefix(1);
      if (!frac.empty() && ('0' <= frac[0] && frac[0] <= '9')) {
        // Now we are sure the fractional part actually exists. Consume it and
        // update s.
        frac.remove_prefix(1);
        while (!frac.empty()) {
          if ('0' <= frac[0] && frac[0] <= '9')
            frac.remove_prefix(1);
          else
            break;
        }
        s = frac;
      }
    }
  }

  // Consume optional exponential part lazily.
  {
    std::string_view exp = s;
    if (!exp.empty() && (exp[0] == 'e' || exp[0] == 'E')) {
      exp.remove_prefix(1);
      if (!exp.empty() && (exp[0] == '+' || exp[0] == '-')) {
        exp.remove_prefix(1);
      }
      if (!exp.empty() && ('0' <= exp[0] && exp[0] <= '9')) {
        // Now we are sure the exponentional part actually exists.
        // Consume it and update s.
        exp.remove_prefix(1);
        while (!exp.empty()) {
          if ('0' <= exp[0] && exp[0] <= '9')
            exp.remove_prefix(1);
          else
            break;
        }
        s = exp;
      }
    }
  }

  double out;
  assert(std::from_chars(orig.data(), s.data(), out).ptr == s.data());
  return {{out, s}};
}

std::optional<std::pair<Object, std::string_view>> ParseObject(
    std::string_view s) {
  if (s.empty() || s[0] != '{') return std::nullopt;
  s.remove_prefix(1);
  s = SkipWS(s);
  if (s.empty()) return std::nullopt;

  Object out;
  auto consume_entry = [&]() {
    auto key_opt = ParseString(s);
    if (!key_opt) return true;
    s = key_opt->second;
    s = SkipWS(s);

    if (s.empty() || s[0] != ':') return true;
    s.remove_prefix(1);
    s = SkipWS(s);

    auto value_opt = ParseValue(s);
    if (!value_opt) return true;
    s = value_opt->second;

    out.insert_or_assign(
        std::move(key_opt->first),
        std::make_unique<ValueWrapper>(std::move(value_opt->first)));
    return false;
  };

  if (s[0] == '}') {
    s.remove_prefix(1);
    return {{std::move(out), s}};
  }
  if (consume_entry()) {
    return std::nullopt;
  }

  s = SkipWS(s);

  while (!s.empty()) {
    if (s[0] == '}') {
      s.remove_prefix(1);
      return {{std::move(out), s}};
    } else if (s[0] == ',') {
      s.remove_prefix(1);
    } else {
      return std::nullopt;
    }
    s = SkipWS(s);

    if (consume_entry()) {
      return std::nullopt;
    }
    s = SkipWS(s);
  }

  return std::nullopt;
}

std::optional<std::pair<Array, std::string_view>> ParseArray(
    std::string_view s) {
  if (s.empty() || s[0] != '[') return std::nullopt;
  s.remove_prefix(1);
  s = SkipWS(s);
  if (s.empty()) return std::nullopt;

  Array out;
  if (s[0] == ']') {
    s.remove_prefix(1);
    return {{std::move(out), s}};
  }
  if (auto opt = ParseValue(s)) {
    out.push_back(std::make_unique<ValueWrapper>(std::move(opt->first)));
    s = opt->second;
  } else {
    return std::nullopt;
  }
  s = SkipWS(s);

  while (!s.empty()) {
    if (s[0] == ']') {
      s.remove_prefix(1);
      return {{std::move(out), s}};
    } else if (s[0] == ',') {
      s.remove_prefix(1);
    } else {
      return std::nullopt;
    }
    s = SkipWS(s);

    if (auto opt = ParseValue(s)) {
      out.push_back(std::make_unique<ValueWrapper>(std::move(opt->first)));
      s = opt->second;
    } else {
      return std::nullopt;
    }
    s = SkipWS(s);
  }

  return std::nullopt;
}

std::optional<std::pair<Value, std::string_view>> ParseValue(
    std::string_view s) {
  if (s.empty()) return std::nullopt;

  Value out;
  switch (s[0]) {
    case '{':
      if (auto object_opt = ParseObject(s); object_opt) {
        out = Value{std::move(object_opt->first)};
        s = object_opt->second;
        break;
      } else {
        return std::nullopt;
      }
    case '[':
      if (auto array_opt = ParseArray(s); array_opt) {
        out = Value{std::move(array_opt->first)};
        s = array_opt->second;
        break;
      } else {
        return std::nullopt;
      }
    case '"':
      if (auto str_opt = ParseString(s); str_opt) {
        out = Value{std::move(str_opt->first)};
        s = str_opt->second;
        break;
      } else {
        return std::nullopt;
      }

    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      if (auto num_opt = ParseNumber(s); num_opt) {
        out = Value{std::move(num_opt->first)};
        s = num_opt->second;
        break;
      } else {
        return std::nullopt;
      }

    case 'f':
      if (s.starts_with("false"sv)) {
        out = Value{false};
        s.remove_prefix("false"sv.size());
        break;
      } else {
        return std::nullopt;
      }
    case 'n':
      if (s.starts_with("null"sv)) {
        out = Value{nullptr};
        s.remove_prefix("null"sv.size());
        break;
      } else {
        return std::nullopt;
      }
    case 't':
      if (s.starts_with("true"sv)) {
        out = Value{true};
        s.remove_prefix("true"sv.size());
        break;
      } else {
        return std::nullopt;
      }

    default:
      return std::nullopt;
  }

  return {{std::move(out), s}};
}

}  // namespace internal
}  // namespace json
