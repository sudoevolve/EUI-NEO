#include "core/render/shadertoy.h"

#include "eui/json.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace core::render {
namespace {

bool integer(const eui::json::Value& value, int& output) {
    std::int64_t signedValue = 0;
    std::uint64_t unsignedValue = 0;
    if (value.signedInteger(signedValue)) {
        output = static_cast<int>(signedValue);
        return true;
    }
    if (value.unsignedInteger(unsignedValue)) {
        output = static_cast<int>(unsignedValue);
        return true;
    }
    return false;
}


void setError(ShaderToyError& error,
              const std::string& source,
              std::string message) {
    error = {ShaderToyErrorCode::SourceReadFailed, {}, {}, "graph-json",
             source, 0, std::move(message)};
}

std::string resolvedPath(const std::filesystem::path& base,
                         const std::string& value) {
    if (value.empty()) return {};
    const std::filesystem::path path = std::filesystem::u8path(value);
    return (path.is_absolute() ? path : base / path).lexically_normal().u8string();
}

bool parseCompactChannel(const eui::json::Value& value,
                         const std::filesystem::path& base,
                         ShaderToyChannel& channel,
                         ShaderToyError& error) {
    std::string spec;
    if (!value.string(spec) || spec.empty()) {
        setError(error, {}, "Compact channel must be a non-empty string.");
        return false;
    }
    if (spec == "none") {
        channel = ShaderToyChannel::none();
    } else if (spec == "self") {
        channel = ShaderToyChannel::self();
    } else if (spec.rfind("image:", 0) == 0) {
        const std::string path = spec.substr(6);
        if (path.empty()) {
            setError(error, {}, "Compact image channel requires a path.");
            return false;
        }
        channel = ShaderToyChannel::image(resolvedPath(base, path));
    } else if (spec.rfind("buffer:", 0) == 0) {
        const std::string passName = spec.substr(7);
        if (passName.empty()) {
            setError(error, {}, "Compact buffer channel requires a pass name.");
            return false;
        }
        channel = ShaderToyChannel::buffer(passName);
    } else {
        channel = ShaderToyChannel::buffer(spec);
    }
    return true;
}

bool parseEuiGraph(const eui::json::Value& root,
                   const eui::json::Value& passes,
                   const std::filesystem::path& base,
                   ShaderToyGraph& graph,
                   ShaderToyError& error) {
    if (root.get("version").valid()) {
        setError(error, {},
                 "EUI Shadertoy graphs do not use a version field.");
        return false;
    }
    graph = {};
    for (std::size_t passIndex = 0; passIndex < passes.size(); ++passIndex) {
        const eui::json::Value value = passes.at(passIndex);
        if (value.type() != eui::json::Type::Object) {
            setError(error, {}, "EUI graph passes must be objects.");
            return false;
        }
        std::string name;
        std::string source;
        std::string inlineSource;
        std::string spirv;
        std::string sourceName;
        if (!value.get("name").string(name)) {
            setError(error, {}, "EUI graph pass requires a name.");
            return false;
        }
        value.get("source").string(source);
        value.get("inlineSource").string(inlineSource);
        value.get("spirv").string(spirv);
        value.get("sourceName").string(sourceName);
        if (!source.empty() && !inlineSource.empty()) {
            setError(error, {},
                     "A pass cannot define both source and inlineSource.");
            return false;
        }
        if (!inlineSource.empty()) {
            graph.addInlinePass(name, inlineSource,
                                resolvedPath(base, spirv),
                                sourceName);
        } else {
            graph.addPass(name, resolvedPath(base, source),
                          resolvedPath(base, spirv));
        }
        const eui::json::Value channels = value.get("channels");
        if (channels.valid()) {
            if (channels.type() != eui::json::Type::Object) {
                setError(error, {},
                         "Pass channels must be an object keyed by 0-3.");
                return false;
            }
            for (std::size_t channelIndex = 0;
                 channelIndex < kShaderToyChannelCount;
                 ++channelIndex) {
                const eui::json::Value channelValue =
                    channels.get(std::to_string(channelIndex));
                if (!channelValue.valid()) continue;
                ShaderToyChannel channel;
                if (!parseCompactChannel(channelValue, base,
                                         channel, error)) {
                    return false;
                }
                graph.setChannel(name, channelIndex, std::move(channel));
            }
        }
    }
    const eui::json::Value uniforms = root.get("uniforms");
    if (uniforms.valid()) {
        if (uniforms.type() != eui::json::Type::Array) {
            setError(error, {}, "EUI graph uniforms must be an array.");
            return false;
        }
        for (std::size_t index = 0; index < uniforms.size(); ++index) {
            const eui::json::Value value = uniforms.at(index);
            std::string name;
            std::string type;
            if (value.type() != eui::json::Type::Object ||
                !value.get("name").string(name) ||
                !value.get("type").string(type)) {
                setError(error, {},
                         "Each EUI graph uniform requires name and type.");
                return false;
            }
            const eui::json::Value components = value.get("value");
            auto number = [&](std::size_t component, float& output) {
                double parsed = 0.0;
                if (components.type() == eui::json::Type::Array) {
                    if (!components.at(component).number(parsed)) return false;
                } else if (component == 0) {
                    if (!components.number(parsed)) return false;
                } else {
                    return false;
                }
                output = static_cast<float>(parsed);
                return true;
            };
            float values[4]{};
            if (type == "float") {
                if (!number(0, values[0])) {
                    setError(error, {}, "Float uniform value must be numeric.");
                    return false;
                }
                graph.setUniform(name, values[0]);
            } else if (type == "int") {
                int parsed = 0;
                if (!integer(components, parsed)) {
                    setError(error, {}, "Int uniform value must be an integer.");
                    return false;
                }
                graph.setUniform(name, parsed);
            } else if (type == "vec2") {
                if (!number(0, values[0]) || !number(1, values[1])) {
                    setError(error, {}, "vec2 uniform value requires two numbers.");
                    return false;
                }
                graph.setUniform(name, Vec2{values[0], values[1]});
            } else if (type == "vec3") {
                if (!number(0, values[0]) || !number(1, values[1]) ||
                    !number(2, values[2])) {
                    setError(error, {}, "vec3 uniform value requires three numbers.");
                    return false;
                }
                graph.setUniform(
                    name, Vec3{values[0], values[1], values[2]});
            } else if (type == "vec4") {
                if (!number(0, values[0]) || !number(1, values[1]) ||
                    !number(2, values[2]) || !number(3, values[3])) {
                    setError(error, {}, "vec4 uniform value requires four numbers.");
                    return false;
                }
                graph.setUniform(
                    name, Color{values[0], values[1],
                                values[2], values[3]});
            } else {
                setError(error, {}, "Unknown EUI graph uniform type: " + type);
                return false;
            }
        }
    }
    return true;
}

} // namespace

bool parseShaderToyGraphJson(const std::string& json,
                             const std::string& baseDirectory,
                             ShaderToyGraph& graph,
                             ShaderToyError& error) {
    eui::json::Document document;
    if (!document.parse(json)) {
        setError(error, {}, document.error().message);
        error.line = static_cast<int>(document.error().offset);
        return false;
    }
    const eui::json::Value root = document.root();
    const eui::json::Value passes = root.get("passes");
    if (passes.type() != eui::json::Type::Array) {
        setError(error, {}, "Shadertoy graph JSON requires a passes array.");
        return false;
    }
    const std::filesystem::path base =
        std::filesystem::u8path(baseDirectory);
    if (!parseEuiGraph(root, passes, base, graph, error)) return false;
    const ShaderToyValidationResult validation =
        validateShaderToyGraph(graph);
    if (!validation.valid()) {
        error = validation.errors.front();
        return false;
    }
    error = {};
    return true;
}

bool loadShaderToyGraphJson(const std::string& path,
                            ShaderToyGraph& graph,
                            ShaderToyError& error) {
    std::ifstream input(std::filesystem::u8path(path), std::ios::binary);
    if (!input) {
        setError(error, path, "Unable to read Shadertoy graph JSON.");
        return false;
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    const std::filesystem::path parent =
        std::filesystem::u8path(path).parent_path();
    if (!parseShaderToyGraphJson(contents.str(), parent.u8string(),
                                 graph, error)) {
        if (error.sourcePath.empty()) error.sourcePath = path;
        return false;
    }
    return true;
}

} // namespace core::render
