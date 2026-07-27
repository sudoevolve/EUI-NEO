#include "eui/json.h"

#include <yyjson.h>

namespace eui::json {

struct Document::Impl {
    yyjson_doc* document = nullptr;
    ParseError error;

    ~Impl() {
        yyjson_doc_free(document);
    }
};

Document::Document() : impl_(std::make_unique<Impl>()) {}
Document::~Document() = default;
Document::Document(Document&&) noexcept = default;
Document& Document::operator=(Document&&) noexcept = default;

bool Document::parse(const std::string& text) {
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }
    yyjson_doc_free(impl_->document);
    impl_->document = nullptr;
    impl_->error = {};

    yyjson_read_err error{};
    impl_->document = yyjson_read_opts(const_cast<char*>(text.data()), text.size(), 0, nullptr, &error);
    if (!impl_->document) {
        impl_->error.code = static_cast<int>(error.code);
        impl_->error.offset = error.pos;
        impl_->error.message = error.msg ? error.msg : "JSON parse failed.";
    }
    return impl_->document != nullptr;
}

bool Document::valid() const {
    return impl_ && impl_->document != nullptr;
}

const ParseError& Document::error() const {
    static const ParseError empty;
    return impl_ ? impl_->error : empty;
}

Value Document::root() const {
    return Value(valid() ? yyjson_doc_get_root(impl_->document) : nullptr);
}

Value Document::atPointer(const std::string& jsonPointer) const {
    return root().atPointer(jsonPointer);
}

bool Document::stringAt(const std::string& jsonPointer, std::string& output) const {
    return atPointer(jsonPointer).string(output);
}

std::string Document::stringAt(const std::string& jsonPointer) const {
    std::string output;
    stringAt(jsonPointer, output);
    return output;
}

bool Value::valid() const {
    return value_ != nullptr;
}

Type Value::type() const {
    auto* value = static_cast<yyjson_val*>(value_);
    if (!value) return Type::Invalid;
    if (yyjson_is_null(value)) return Type::Null;
    if (yyjson_is_bool(value)) return Type::Boolean;
    if (yyjson_is_sint(value)) return Type::SignedInteger;
    if (yyjson_is_uint(value)) return Type::UnsignedInteger;
    if (yyjson_is_real(value)) return Type::Real;
    if (yyjson_is_str(value)) return Type::String;
    if (yyjson_is_arr(value)) return Type::Array;
    if (yyjson_is_obj(value)) return Type::Object;
    return Type::Invalid;
}

bool Value::isNull() const {
    return yyjson_is_null(static_cast<yyjson_val*>(value_));
}

std::size_t Value::size() const {
    auto* value = static_cast<yyjson_val*>(value_);
    if (yyjson_is_arr(value)) return yyjson_arr_size(value);
    if (yyjson_is_obj(value)) return yyjson_obj_size(value);
    return 0;
}

bool Value::boolean(bool& output) const {
    auto* value = static_cast<yyjson_val*>(value_);
    if (!yyjson_is_bool(value)) return false;
    output = yyjson_get_bool(value);
    return true;
}

bool Value::signedInteger(std::int64_t& output) const {
    auto* value = static_cast<yyjson_val*>(value_);
    if (!yyjson_is_sint(value)) return false;
    output = yyjson_get_sint(value);
    return true;
}

bool Value::unsignedInteger(std::uint64_t& output) const {
    auto* value = static_cast<yyjson_val*>(value_);
    if (!yyjson_is_uint(value)) return false;
    output = yyjson_get_uint(value);
    return true;
}

bool Value::number(double& output) const {
    auto* value = static_cast<yyjson_val*>(value_);
    if (!yyjson_is_num(value)) return false;
    output = yyjson_get_num(value);
    return true;
}

bool Value::string(std::string& output) const {
    output.clear();
    auto* value = static_cast<yyjson_val*>(value_);
    if (!yyjson_is_str(value)) return false;
    output.assign(yyjson_get_str(value), yyjson_get_len(value));
    return true;
}

Value Value::at(std::size_t index) const {
    auto* value = static_cast<yyjson_val*>(value_);
    return Value(yyjson_is_arr(value) ? yyjson_arr_get(value, index) : nullptr);
}

Value Value::get(const std::string& key) const {
    auto* value = static_cast<yyjson_val*>(value_);
    return Value(yyjson_is_obj(value) ? yyjson_obj_getn(value, key.data(), key.size()) : nullptr);
}

Value Value::atPointer(const std::string& jsonPointer) const {
    auto* value = static_cast<yyjson_val*>(value_);
    return Value(value ? yyjson_ptr_get(value, jsonPointer.c_str()) : nullptr);
}

} // namespace eui::json
