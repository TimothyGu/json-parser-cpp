#ifndef PARSE_H
#define PARSE_H

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "json.h"

namespace json {
namespace internal {

std::string_view SkipWS(std::string_view);

std::optional<std::pair<std::string, std::string_view>> ParseString(
    std::string_view);
std::optional<std::pair<double, std::string_view>> ParseNumber(
    std::string_view);
std::optional<std::pair<Object, std::string_view>> ParseObject(
    std::string_view);
std::optional<std::pair<Array, std::string_view>> ParseArray(std::string_view);
std::optional<std::pair<Value, std::string_view>> ParseValue(std::string_view);

}  // namespace internal
}  // namespace json

#endif
