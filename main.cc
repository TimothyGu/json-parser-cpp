#include <iostream>
#include <memory>
#include <optional>
#include <string>

#include "json.h"

int main(int argc, char* argv[]) {
  std::cout << "Value: " << sizeof(json::Value) << '\n';
  std::cout << "Object: " << sizeof(json::Object) << '\n';
  std::cout << "Array: " << sizeof(json::Array) << '\n';
  std::cout << "std::unique_ptr<Value>: "
            << sizeof(std::unique_ptr<json::Value>) << '\n';
  std::cout << "std::string: " << sizeof(std::string) << '\n';

  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " <text_to_parse>\n";
    return 1;
  }

  if (std::optional<json::Value> out = json::Parse(argv[1])) {
    json::Print(*out);
    json::Print(json::Clone(*out));
  } else {
    std::cerr << "error: failed to parse\n";
    return 2;
  }
}
