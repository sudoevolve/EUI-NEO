#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vcd {

struct ValueChange {
    std::uint64_t time = 0;
    std::string value;
};

struct Signal {
    std::string code;
    std::string name;
    std::string kind;
    int width = 1;
    std::vector<ValueChange> changes;
};

struct Document {
    std::string path;
    std::string timescale = "ticks";
    std::uint64_t endTime = 0;
    std::vector<Signal> signals;
};

bool loadFile(const std::string& path, Document& document, std::string& error);
std::string valueAt(const Signal& signal, std::uint64_t time);
bool containsInsensitive(const std::string& value, const std::string& query);

} // namespace vcd
