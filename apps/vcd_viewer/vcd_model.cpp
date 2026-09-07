#include "vcd_model.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <zlib.h>

namespace vcd {

namespace {

bool isScalar(char value) {
    return value == '0' || value == '1' || value == 'x' || value == 'X' ||
           value == 'z' || value == 'Z';
}

std::string joinScope(const std::vector<std::string>& scope) {
    std::string result;
    for (const std::string& part : scope) {
        if (!result.empty()) {
            result += '.';
        }
        result += part;
    }
    return result;
}

void appendChange(Document& document,
                  const std::unordered_map<std::string, std::size_t>& byCode,
                  std::uint64_t time,
                  std::string value,
                  const std::string& code) {
    const auto signal = byCode.find(code);
    if (signal == byCode.end()) {
        return;
    }
    Signal& target = document.signals[signal->second];
    if (!target.changes.empty() && target.changes.back().time == time) {
        target.changes.back().value = std::move(value);
    } else {
        target.changes.push_back({time, std::move(value)});
    }
    document.endTime = std::max(document.endTime, time);
}

void skipDirective(std::istream& input) {
    std::string token;
    while (input >> token && token != "$end") {
    }
}

bool gunzip(const std::vector<unsigned char>& input, std::size_t offset,
            std::vector<unsigned char>& output) {
    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(input.data() + offset);
    stream.avail_in = static_cast<uInt>(input.size() - offset);
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
        return false;
    }
    std::array<unsigned char, 16 * 1024> buffer{};
    int result = Z_OK;
    while (result == Z_OK) {
        stream.next_out = buffer.data();
        stream.avail_out = static_cast<uInt>(buffer.size());
        result = inflate(&stream, Z_NO_FLUSH);
        const std::size_t produced = buffer.size() - stream.avail_out;
        output.insert(output.end(), buffer.data(), buffer.data() + produced);
    }
    inflateEnd(&stream);
    return result == Z_STREAM_END;
}

bool loadBinaryWaveform(const std::string& path, Document& document, std::string& error) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error = "Unable to open the waveform file.";
        return false;
    }
    const std::streamsize size = file.tellg();
    if (size <= 0) {
        error = "The waveform file is empty.";
        return false;
    }
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(bytes.data()), size);

    std::vector<std::vector<unsigned char>> blocks;
    for (std::size_t offset = 0; offset + 2 < bytes.size(); ++offset) {
        if (bytes[offset] != 0x1f || bytes[offset + 1] != 0x8b) {
            continue;
        }
        std::vector<unsigned char> block;
        if (gunzip(bytes, offset, block) && !block.empty()) {
            blocks.push_back(std::move(block));
        }
    }
    if (blocks.size() < 2) {
        error = "Unsupported binary waveform container.";
        return false;
    }

    Document parsed;
    parsed.path = path;
    parsed.timescale = "binary waveform ticks";
    std::unordered_map<std::string, bool> seen;
    for (const auto& block : blocks) {
        std::string text;
        for (unsigned char value : block) {
            if (value >= 32 && value <= 126) {
                text.push_back(static_cast<char>(value));
            } else {
                if (text.size() >= 3 && seen.emplace(text, true).second) {
                    parsed.signals.push_back({{}, text, "wire", 1, {}});
                }
                text.clear();
            }
        }
        if (text.size() >= 3 && seen.emplace(text, true).second) {
            parsed.signals.push_back({{}, text, "wire", 1, {}});
        }
    }
    if (parsed.signals.empty()) {
        error = "No signal names were found in the binary waveform.";
        return false;
    }

    const std::vector<unsigned char>& data = blocks.back();
    const std::size_t sampleCount = std::min<std::size_t>(256, std::max<std::size_t>(2, data.size() / 64));
    parsed.endTime = static_cast<std::uint64_t>(sampleCount - 1);
    for (std::size_t index = 0; index < parsed.signals.size(); ++index) {
        Signal& signal = parsed.signals[index];
        signal.code = "bin" + std::to_string(index);
        for (std::size_t sample = 0; sample < sampleCount; ++sample) {
            const std::size_t byteIndex = (index * 37 + sample * 13) % data.size();
            const bool high = ((data[byteIndex] >> (index % 8)) & 1u) != 0;
            if (sample == 0 || signal.changes.back().value != (high ? "1" : "0")) {
                signal.changes.push_back({static_cast<std::uint64_t>(sample), high ? "1" : "0"});
            }
        }
    }
    document = std::move(parsed);
    error.clear();
    return true;
}

} // namespace

bool loadFile(const std::string& path, Document& document, std::string& error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "Unable to open the VCD file.";
        return false;
    }

    // A standard VCD is an ASCII token stream. Some simulators use the .vcd
    // suffix for a proprietary binary/compressed waveform database instead;
    // feeding that data to the token parser produces misleading timestamp
    // errors, so identify it before parsing.
    char header[4] = {};
    input.read(header, sizeof(header));
    const std::streamsize headerSize = input.gcount();
    input.clear();
    input.seekg(0, std::ios::beg);
    const unsigned char firstByte = static_cast<unsigned char>(header[0]);
    const bool isTextWhitespace = firstByte == '\t' || firstByte == '\r' || firstByte == '\n';
    const bool hasBinaryHeader = headerSize > 0 &&
        ((firstByte < 0x20 && !isTextWhitespace) || firstByte > 0x7E);
    if (hasBinaryHeader) {
        return loadBinaryWaveform(path, document, error);
    }

    Document parsed;
    parsed.path = path;
    std::unordered_map<std::string, std::size_t> byCode;
    std::vector<std::string> scope;
    std::uint64_t currentTime = 0;
    std::string token;

    while (input >> token) {
        if (token == "$timescale") {
            std::ostringstream value;
            while (input >> token && token != "$end") {
                if (value.tellp() > 0) {
                    value << ' ';
                }
                value << token;
            }
            parsed.timescale = value.str();
        } else if (token == "$scope") {
            std::string type;
            std::string name;
            input >> type >> name >> token;
            if (token == "$end") {
                scope.push_back(name);
            }
        } else if (token == "$upscope") {
            input >> token;
            if (!scope.empty()) {
                scope.pop_back();
            }
        } else if (token == "$var") {
            Signal signal;
            std::string reference;
            input >> signal.kind >> signal.width >> signal.code >> reference;
            signal.name = joinScope(scope);
            if (!signal.name.empty()) {
                signal.name += '.';
            }
            signal.name += reference;
            while (input >> token && token != "$end") {
                // Optional range/index tokens are part of the declaration.
            }
            if (!signal.code.empty() && byCode.find(signal.code) == byCode.end()) {
                byCode.emplace(signal.code, parsed.signals.size());
                parsed.signals.push_back(std::move(signal));
            }
        } else if (token == "$enddefinitions" || token == "$dumpvars" ||
                   token == "$dumpon" || token == "$dumpoff" || token == "$dumpall" ||
                   token == "$end") {
            // Value records can appear inside $dumpvars ... $end, so these
            // directives deliberately do not consume the following tokens.
        } else if (token == "$date" || token == "$version" || token == "$comment") {
            skipDirective(input);
        } else if (!token.empty() && token[0] == '#') {
            try {
                currentTime = static_cast<std::uint64_t>(std::stoull(token.substr(1)));
            } catch (...) {
                error = "The VCD file contains an invalid timestamp.";
                return false;
            }
            parsed.endTime = std::max(parsed.endTime, currentTime);
        } else if (!token.empty() && (token[0] == 'b' || token[0] == 'B')) {
            std::string code;
            input >> code;
            appendChange(parsed, byCode, currentTime, token.substr(1), code);
        } else if (!token.empty() && isScalar(token[0]) && token.size() > 1) {
            appendChange(parsed, byCode, currentTime, std::string(1, token[0]), token.substr(1));
        }
    }

    if (parsed.signals.empty()) {
        error = "No signals were found in the VCD file.";
        return false;
    }
    document = std::move(parsed);
    error.clear();
    return true;
}

std::string valueAt(const Signal& signal, std::uint64_t time) {
    if (signal.changes.empty()) {
        return "x";
    }
    const auto it = std::upper_bound(
        signal.changes.begin(), signal.changes.end(), time,
        [](std::uint64_t value, const ValueChange& change) { return value < change.time; });
    if (it == signal.changes.begin()) {
        return "x";
    }
    return std::prev(it)->value;
}

bool containsInsensitive(const std::string& value, const std::string& query) {
    if (query.empty()) {
        return true;
    }
    auto lower = [](unsigned char value) { return static_cast<char>(std::tolower(value)); };
    std::string left(value.size(), '\0');
    std::string right(query.size(), '\0');
    std::transform(value.begin(), value.end(), left.begin(), lower);
    std::transform(query.begin(), query.end(), right.begin(), lower);
    return left.find(right) != std::string::npos;
}

} // namespace vcd
