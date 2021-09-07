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

struct ValueWrapper;

using Object = std::unordered_map<std::string, std::unique_ptr<ValueWrapper>>;
using Array = std::vector<std::unique_ptr<ValueWrapper>>;

// Value is move-only.
using Value =
    std::variant<std::nullptr_t, bool, double, std::string, Object, Array>;

// A wrapper for Value to allow recursive types.
// Just like Value, ValueWrapper is move-only.
struct ValueWrapper {
  Value v;

  ValueWrapper() = default;
  ValueWrapper(ValueWrapper&&) = default;
  ValueWrapper(Value&& w) : v(std::move(w)) {}

  constexpr Value& get() noexcept { return v; }
  constexpr operator Value&() noexcept { return v; }
  constexpr const Value& get() const noexcept { return v; }
};

std::optional<Value> Parse(std::string_view);
Value Clone(const Value&);
void Print(const Value&);

}  // namespace json

#endif
