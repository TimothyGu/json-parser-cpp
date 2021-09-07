# Toy C++ JSON parser

**Toy project. Do not use in production.**

This is a JSON parser written in modern C++. Here are some of the more
interesting bits of the API:

```c++
namespace json {

using Object = std::unordered_map<std::string, std::unique_ptr<Value>>;
using Array = std::vector<std::unique_ptr<Value>>;

class Value : public std::variant<std::nullptr_t, bool, double, std::string,
                                  Object, Array> { /* ... */ };

std::optional<Value> Parse(std::string_view);
Value Clone(const Value&);

}  // namespace json
```

Requires a C++ compiler that supports C++20, as well as ICU4C.

## Organization

* main.cc is a test program.
* json.h exposes a public API.
* parse.h and unicode.h contain internal utilities.
