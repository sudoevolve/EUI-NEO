#include "eui/json.h"

#include <iostream>

int main() {
    eui::json::Document document;
    if (!document.parse(R"({"title":"\u4f60\u597d","empty":"","enabled":true,"count":-3,"size":4,"ratio":1.5,"nothing":null,"images":[{"url":"https:\/\/example.com\/a.png","copyright":"Bing image"}]})")) {
        std::cerr << "valid JSON failed to parse\n";
        return 1;
    }
    if (document.stringAt("/title") != "\xE4\xBD\xA0\xE5\xA5\xBD") {
        std::cerr << "unicode string was not decoded\n";
        return 1;
    }
    if (document.stringAt("/images/0/url") != "https://example.com/a.png") {
        std::cerr << "JSON pointer lookup failed\n";
        return 1;
    }
    if (document.stringAt("/images/0/copyright") != "Bing image") {
        std::cerr << "nested object lookup failed\n";
        return 1;
    }
    if (!document.stringAt("/missing").empty()) {
        std::cerr << "missing value should be empty\n";
        return 1;
    }
    std::string empty;
    if (!document.stringAt("/empty", empty) || !empty.empty()) {
        std::cerr << "empty string lookup failed\n";
        return 1;
    }
    if (document.root().type() != eui::json::Type::Object || document.root().size() != 8) {
        std::cerr << "root object metadata failed\n";
        return 1;
    }
    bool enabled = false;
    std::int64_t count = 0;
    std::uint64_t size = 0;
    double ratio = 0.0;
    if (!document.root().get("enabled").boolean(enabled) || !enabled ||
        !document.atPointer("/count").signedInteger(count) || count != -3 ||
        !document.atPointer("/size").unsignedInteger(size) || size != 4 ||
        !document.atPointer("/ratio").number(ratio) || ratio != 1.5 ||
        !document.atPointer("/nothing").isNull() ||
        document.atPointer("/images").at(0).type() != eui::json::Type::Object) {
        std::cerr << "typed JSON access failed\n";
        return 1;
    }
    if (document.parse("{invalid")) {
        std::cerr << "invalid JSON unexpectedly parsed\n";
        return 1;
    }
    if (!document.error() || document.error().message.empty()) {
        std::cerr << "parse error details were not preserved\n";
        return 1;
    }
    return 0;
}
