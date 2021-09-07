#ifndef JSON_H
#define JSON_H

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace json {

class Value;

using Object = std::unordered_map<std::string, std::unique_ptr<Value>>;
using Array = std::vector<std::unique_ptr<Value>>;

// Value is move-only.
class Value : public std::variant<std::nullptr_t, bool, double, std::string,
                                  Object, Array> {
 public:
  using Base =
      std::variant<std::nullptr_t, bool, double, std::string, Object, Array>;

  using variant::variant;
};

std::optional<Value> Parse(std::string_view);
Value Clone(const Value&);
void Print(const Value&);

// Same as std::visit(visitor, val) in C++23.
// See http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p2162r2.html
constexpr auto Visit(auto visitor, const Value& val) {
  return std::visit(visitor, static_cast<const Value::Base&>(val));
}

}  // namespace json

#endif
