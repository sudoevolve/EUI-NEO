#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace eui::json {

enum class Type {
    Invalid,
    Null,
    Boolean,
    SignedInteger,
    UnsignedInteger,
    Real,
    String,
    Array,
    Object
};

struct ParseError {
    int code = 0;
    std::size_t offset = 0;
    std::string message;

    explicit operator bool() const { return code != 0; }
};

// A non-owning view. Values remain valid until their Document is reparsed or
// the document storage is destroyed.
class Value {
public:
    Value() = default;

    bool valid() const;
    Type type() const;
    bool isNull() const;
    std::size_t size() const;

    bool boolean(bool& output) const;
    bool signedInteger(std::int64_t& output) const;
    bool unsignedInteger(std::uint64_t& output) const;
    bool number(double& output) const;
    bool string(std::string& output) const;

    Value at(std::size_t index) const;
    Value get(const std::string& key) const;
    Value atPointer(const std::string& jsonPointer) const;

private:
    explicit Value(void* value) : value_(value) {}
    void* value_ = nullptr;

    friend class Document;
};

class Document {
public:
    Document();
    ~Document();

    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;
    Document(Document&&) noexcept;
    Document& operator=(Document&&) noexcept;

    bool parse(const std::string& text);
    bool valid() const;
    const ParseError& error() const;
    Value root() const;
    Value atPointer(const std::string& jsonPointer) const;

    bool stringAt(const std::string& jsonPointer, std::string& output) const;
    std::string stringAt(const std::string& jsonPointer) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace eui::json
