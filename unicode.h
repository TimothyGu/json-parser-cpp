#ifndef UNICODE_H
#define UNICODE_H

#include <unicode/utf16.h>
#include <unicode/utf8.h>

#include <string_view>
#include <tuple>
#include <utility>

namespace json {
namespace internal {

namespace utf8 {

std::pair<char32_t, std::string_view> Decode(std::string_view s);

std::string_view InternalEncode(char32_t, char*);

template <std::size_t sz>
std::string_view Encode(char32_t cp, char (&buf)[sz]) {
  static_assert(sz >= U8_MAX_LENGTH,
                "provided scratch buffer must be at least U8_MAX_LENGTH wide");
  return InternalEncode(cp, buf);
}

}  // namespace utf8

namespace utf16 {

inline bool IsLeadSurrogate(char16_t cu) { return U16_IS_LEAD(cu); }
inline bool IsTrailingSurrogate(char16_t cu) { return U16_IS_TRAIL(cu); }

inline char32_t Compose(char16_t lead, char16_t trail) {
  return U16_GET_SUPPLEMENTARY(lead, trail);
}

}  // namespace utf16

}  // namespace internal
}  // namespace json

#endif
