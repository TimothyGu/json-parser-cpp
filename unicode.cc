#include "unicode.h"

#include <unicode/utf8.h>

#include <string_view>
#include <tuple>
#include <utility>

namespace json {
namespace internal {
namespace utf8 {

std::pair<char32_t, std::string_view> Decode(std::string_view s) {
  char32_t c = 0;
  int i = 0;
  U8_NEXT_OR_FFFD(s.data(), i, s.length(), c);
  return {c, s.substr(i)};
}

std::string_view InternalEncode(char32_t cp, char* buf) {
  size_t i = 0;
  U8_APPEND(buf, i, sizeof(buf), cp, std::ignore);
  return {buf, i};
}

}  // namespace utf8
}  // namespace internal
}  // namespace json
