#include "core/render/shadertoy.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>
#include <unordered_set>

namespace core::render {
namespace {

constexpr const char* kReservedUniforms[] = {
    "iResolution", "iTime", "iTimeDelta", "iFrame", "iFrameRate", "iDate", "iMouse",
    "iChannel0", "iChannel1", "iChannel2", "iChannel3", "iChannelTime",
    "iChannelResolution", "iSampleRate"
};

bool validIdentifier(const std::string& value) {
    if (value.empty() ||
        (std::isalpha(static_cast<unsigned char>(value.front())) == 0 && value.front() != '_')) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](unsigned char ch) {
        return std::isalnum(ch) != 0 || ch == '_';
    });
}

void hashBytes(std::uint64_t& hash, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
}

void hashString(std::uint64_t& hash, const std::string& value) {
    hashBytes(hash, value.data(), value.size());
    constexpr unsigned char delimiter = 0xffu;
    hashBytes(hash, &delimiter, 1);
}

template <typename T>
void hashValue(std::uint64_t& hash, const T& value) {
    hashBytes(hash, &value, sizeof(value));
}

void setUniform(ShaderToyGraph& graph,
                std::string name,
                ShaderToyUniformKind kind,
                std::array<float, 4> values) {
    const auto found = std::find_if(graph.uniforms.begin(), graph.uniforms.end(),
        [&](const ShaderToyUniform& uniform) { return uniform.name == name; });
    if (found == graph.uniforms.end()) {
        graph.uniforms.push_back({std::move(name), kind, values});
    } else {
        found->kind = kind;
        found->values = values;
    }
}

const char* kOpenGLPrelude = R"GLSL(#version 330 core
out vec4 euiFragColor;
in vec2 euiTexCoord;
uniform vec3 iResolution;
uniform float iTime;
uniform float iTimeDelta;
uniform int iFrame;
uniform float iFrameRate;
uniform vec4 iDate;
uniform vec4 iMouse;
uniform float iChannelTime[4];
uniform vec3 iChannelResolution[4];
uniform float iSampleRate;
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
uniform sampler2D iChannel2;
uniform sampler2D iChannel3;
)GLSL";

const char* kVulkanPrelude = R"GLSL(#version 450
layout(location = 0) in vec2 euiTexCoord;
layout(location = 0) out vec4 euiFragColor;
layout(set = 0, binding = 0, std140) uniform EuiShaderToyUniforms {
    vec4 resolutionTime;
    vec4 timeData;
    vec4 date;
    vec4 mouse;
    vec4 channelTime;
    vec4 channelResolution[4];
    vec4 sampleFrame;
    vec4 customUniforms[16];
} euiUniforms;
#define iResolution euiUniforms.resolutionTime.xyz
#define iTime euiUniforms.resolutionTime.w
#define iTimeDelta euiUniforms.timeData.x
#define iFrameRate euiUniforms.timeData.y
#define iFrame int(euiUniforms.timeData.z)
#define iDate euiUniforms.date
#define iMouse euiUniforms.mouse
#define iSampleRate euiUniforms.sampleFrame.x
float iChannelTime[4];
vec3 iChannelResolution[4];
layout(set = 0, binding = 1) uniform sampler2D iChannel0;
layout(set = 0, binding = 2) uniform sampler2D iChannel1;
layout(set = 0, binding = 3) uniform sampler2D iChannel2;
layout(set = 0, binding = 4) uniform sampler2D iChannel3;
)GLSL";

const char* kOpenGLPostlude = R"GLSL(
void main() {
    vec2 fragCoord = euiTexCoord * iResolution.xy;
    mainImage(euiFragColor, fragCoord);
}
)GLSL";

const char* kVulkanPostlude = R"GLSL(
void main() {
    iChannelTime[0] = euiUniforms.channelTime.x;
    iChannelTime[1] = euiUniforms.channelTime.y;
    iChannelTime[2] = euiUniforms.channelTime.z;
    iChannelTime[3] = euiUniforms.channelTime.w;
    iChannelResolution[0] = euiUniforms.channelResolution[0].xyz;
    iChannelResolution[1] = euiUniforms.channelResolution[1].xyz;
    iChannelResolution[2] = euiUniforms.channelResolution[2].xyz;
    iChannelResolution[3] = euiUniforms.channelResolution[3].xyz;
    vec2 fragCoord = euiTexCoord * iResolution.xy;
    mainImage(euiFragColor, fragCoord);
}
)GLSL";

const char* uniformTypeName(ShaderToyUniformKind kind) {
    switch (kind) {
    case ShaderToyUniformKind::Float: return "float";
    case ShaderToyUniformKind::Vec2: return "vec2";
    case ShaderToyUniformKind::Vec3: return "vec3";
    case ShaderToyUniformKind::Vec4: return "vec4";
    case ShaderToyUniformKind::Int: return "int";
    }
    return "float";
}

std::string customUniformDeclarations(const std::vector<ShaderToyUniform>& uniforms) {
    std::string declarations;
    for (const ShaderToyUniform& uniform : uniforms) {
        declarations += "uniform ";
        declarations += uniformTypeName(uniform.kind);
        declarations += " ";
        declarations += uniform.name;
        declarations += ";\n";
    }
    return declarations;
}

std::string vulkanCustomUniformAliases(const std::vector<ShaderToyUniform>& uniforms) {
    std::string aliases;
    for (std::size_t index = 0; index < uniforms.size(); ++index) {
        aliases += "#define ";
        aliases += uniforms[index].name;
        aliases += " ";
        if (uniforms[index].kind == ShaderToyUniformKind::Int) {
            aliases += "int(euiUniforms.customUniforms[";
            aliases += std::to_string(index);
            aliases += "].x)";
        } else {
            aliases += "euiUniforms.customUniforms[";
            aliases += std::to_string(index);
            aliases += "]";
            switch (uniforms[index].kind) {
            case ShaderToyUniformKind::Float: aliases += ".x"; break;
            case ShaderToyUniformKind::Vec2: aliases += ".xy"; break;
            case ShaderToyUniformKind::Vec3: aliases += ".xyz"; break;
            case ShaderToyUniformKind::Vec4: break;
            case ShaderToyUniformKind::Int: break;
            }
        }
        aliases += "\n";
    }
    return aliases;
}

struct GlslToken {
    std::size_t end = 0;
    std::string_view text;
};

bool glslIdentifierStart(unsigned char value) {
    return std::isalpha(value) != 0 || value == '_';
}

bool glslIdentifierContinue(unsigned char value) {
    return std::isalnum(value) != 0 || value == '_';
}

std::vector<GlslToken> tokenizeGlsl(const std::string& source) {
    std::vector<GlslToken> tokens;
    bool lineStart = true;
    for (std::size_t index = 0; index < source.size();) {
        const unsigned char value = static_cast<unsigned char>(source[index]);
        if (source[index] == '\n') {
            lineStart = true;
            ++index;
            continue;
        }
        if (std::isspace(value) != 0) {
            ++index;
            continue;
        }
        if (lineStart && source[index] == '#') {
            while (index < source.size() && source[index] != '\n') ++index;
            continue;
        }
        lineStart = false;
        if (source[index] == '/' && index + 1 < source.size()) {
            if (source[index + 1] == '/') {
                index += 2;
                while (index < source.size() && source[index] != '\n') ++index;
                continue;
            }
            if (source[index + 1] == '*') {
                index += 2;
                while (index + 1 < source.size() &&
                       (source[index] != '*' || source[index + 1] != '/')) {
                    if (source[index] == '\n') lineStart = true;
                    ++index;
                }
                index = std::min(source.size(), index + 2);
                continue;
            }
        }
        if (source[index] == '"') {
            const std::size_t begin = index++;
            while (index < source.size()) {
                if (source[index] == '\\' && index + 1 < source.size()) {
                    index += 2;
                } else if (source[index++] == '"') {
                    break;
                }
            }
            tokens.push_back({index,
                              std::string_view(source).substr(begin, index - begin)});
            continue;
        }
        if (glslIdentifierStart(value)) {
            const std::size_t begin = index++;
            while (index < source.size() &&
                   glslIdentifierContinue(static_cast<unsigned char>(source[index]))) {
                ++index;
            }
            tokens.push_back({index,
                              std::string_view(source).substr(begin, index - begin)});
            continue;
        }
        tokens.push_back({index + 1,
                          std::string_view(source).substr(index, 1)});
        ++index;
    }
    return tokens;
}

bool glslLocalType(std::string_view value) {
    static constexpr std::string_view types[] = {
        "bool", "int", "uint", "float", "double",
        "bvec2", "bvec3", "bvec4", "ivec2", "ivec3", "ivec4",
        "uvec2", "uvec3", "uvec4", "vec2", "vec3", "vec4",
        "dvec2", "dvec3", "dvec4", "mat2", "mat3", "mat4",
        "mat2x2", "mat2x3", "mat2x4", "mat3x2", "mat3x3",
        "mat3x4", "mat4x2", "mat4x3", "mat4x4",
        "dmat2", "dmat3", "dmat4", "dmat2x2", "dmat2x3",
        "dmat2x4", "dmat3x2", "dmat3x3", "dmat3x4", "dmat4x2",
        "dmat4x3", "dmat4x4"
    };
    return std::find(std::begin(types), std::end(types), value) != std::end(types);
}

bool glslDeclarationQualifier(std::string_view value) {
    return value == "const" || value == "lowp" ||
           value == "mediump" || value == "highp" ||
           value == "precise";
}

bool declarationStart(const std::vector<GlslToken>& tokens, std::size_t typeIndex) {
    std::size_t index = typeIndex;
    while (index > 0 && glslDeclarationQualifier(tokens[index - 1].text)) --index;
    if (index == 0) return false;
    const std::string_view previous = tokens[index - 1].text;
    if (previous == "{" || previous == ";" || previous == "}") return true;
    return previous == "(" && index >= 2 && tokens[index - 2].text == "for";
}

bool identifierToken(const GlslToken& token) {
    return !token.text.empty() &&
           glslIdentifierStart(static_cast<unsigned char>(token.text.front()));
}

struct SourceInsertion {
    std::size_t offset = 0;
    std::string text;
};

void appendDeclaratorInitialization(
    const std::vector<GlslToken>& tokens,
    std::size_t begin,
    std::size_t end,
    std::string_view type,
    std::vector<SourceInsertion>& insertions) {
    if (begin >= end || !identifierToken(tokens[begin])) return;
    int parenthesis = 0;
    int brackets = 0;
    int braces = 0;
    bool initialized = false;
    bool array = false;
    for (std::size_t index = begin + 1; index < end; ++index) {
        const std::string_view token = tokens[index].text;
        if (token == "(" ) ++parenthesis;
        else if (token == ")") --parenthesis;
        else if (token == "[") {
            if (parenthesis == 0 && brackets == 0 && braces == 0) array = true;
            ++brackets;
        } else if (token == "]") --brackets;
        else if (token == "{") ++braces;
        else if (token == "}") --braces;
        else if (token == "=" && parenthesis == 0 && brackets == 0 && braces == 0) {
            initialized = true;
        }
    }
    if (!initialized && !array) {
        insertions.push_back({
            tokens[begin].end,
            " = " + std::string(type) + "(0)"
        });
    }
}

std::vector<SourceInsertion> localInitializations(
    const std::vector<GlslToken>& tokens) {
    std::vector<SourceInsertion> insertions;
    int globalBraceDepth = 0;
    int functionDepth = 0;
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        const std::string_view token = tokens[index].text;
        if (token == "{") {
            if (functionDepth > 0) {
                ++functionDepth;
            } else if (globalBraceDepth == 0 && index > 0 &&
                       tokens[index - 1].text == ")") {
                functionDepth = 1;
            }
            ++globalBraceDepth;
            continue;
        }
        if (token == "}") {
            if (functionDepth > 0) --functionDepth;
            if (globalBraceDepth > 0) --globalBraceDepth;
            continue;
        }
        if (functionDepth == 0 || !glslLocalType(token) ||
            !declarationStart(tokens, index)) {
            continue;
        }
        const std::size_t firstDeclarator = index + 1;
        if (firstDeclarator >= tokens.size()) continue;
        int parenthesis = 0;
        int brackets = 0;
        int braces = 0;
        std::size_t declaratorBegin = firstDeclarator;
        for (std::size_t cursor = firstDeclarator; cursor < tokens.size(); ++cursor) {
            const std::string_view current = tokens[cursor].text;
            if (current == "(") ++parenthesis;
            else if (current == ")") --parenthesis;
            else if (current == "[") ++brackets;
            else if (current == "]") --brackets;
            else if (current == "{") ++braces;
            else if (current == "}") --braces;
            const bool boundary = parenthesis == 0 && brackets == 0 && braces == 0 &&
                                  (current == "," || current == ";");
            if (!boundary) continue;
            appendDeclaratorInitialization(tokens, declaratorBegin, cursor,
                                            token, insertions);
            declaratorBegin = cursor + 1;
            if (current == ";") {
                index = cursor;
                break;
            }
        }
    }
    return insertions;
}

} // namespace

ShaderToyChannel ShaderToyChannel::none() { return {}; }
ShaderToyChannel ShaderToyChannel::image(std::string path) {
    ShaderToyChannel result;
    result.kind = ShaderToyChannelKind::Image;
    result.source = std::move(path);
    return result;
}
ShaderToyChannel ShaderToyChannel::buffer(std::string passName) {
    ShaderToyChannel result;
    result.kind = ShaderToyChannelKind::Buffer;
    result.source = std::move(passName);
    return result;
}
ShaderToyChannel ShaderToyChannel::self() {
    ShaderToyChannel result;
    result.kind = ShaderToyChannelKind::Self;
    return result;
}
ShaderToyPass& ShaderToyGraph::addPass(std::string name,
                                      std::string fragmentPath,
                                      std::string spirvPath) {
    if (spirvPath.empty()) {
        spirvPath = shaderToyDefaultSpirvPath(fragmentPath);
    }
    ShaderToyPass pass;
    pass.name = std::move(name);
    pass.fragmentPath = std::move(fragmentPath);
    pass.spirvPath = std::move(spirvPath);
    passes.push_back(std::move(pass));
    return passes.back();
}

ShaderToyPass& ShaderToyGraph::addInlinePass(std::string name,
                                            std::string fragmentSource,
                                            std::string spirvPath,
                                            std::string sourceName) {
    ShaderToyPass pass;
    pass.name = std::move(name);
    pass.sourceKind = ShaderToySourceKind::Inline;
    pass.fragmentPath = sourceName.empty()
        ? std::string("<inline:") + pass.name + ">"
        : std::move(sourceName);
    pass.fragmentSource = std::move(fragmentSource);
    pass.spirvPath = std::move(spirvPath);
    passes.push_back(std::move(pass));
    return passes.back();
}

ShaderToyGraph& ShaderToyGraph::setChannel(const std::string& passName,
                                           std::size_t channel,
                                           ShaderToyChannel input) {
    if (channel < kShaderToyChannelCount) {
        if (ShaderToyPass* pass = findPass(passName)) {
            pass->channels[channel] = std::move(input);
        }
    }
    return *this;
}

ShaderToyGraph& ShaderToyGraph::setUniform(std::string name, float value) {
    ::core::render::setUniform(*this, std::move(name), ShaderToyUniformKind::Float,
                              {value, 0.0f, 0.0f, 0.0f});
    return *this;
}
ShaderToyGraph& ShaderToyGraph::setUniform(std::string name, const Vec2& value) {
    ::core::render::setUniform(*this, std::move(name), ShaderToyUniformKind::Vec2,
                              {value.x, value.y, 0.0f, 0.0f});
    return *this;
}
ShaderToyGraph& ShaderToyGraph::setUniform(std::string name, const Vec3& value) {
    ::core::render::setUniform(*this, std::move(name), ShaderToyUniformKind::Vec3,
                              {value.x, value.y, value.z, 0.0f});
    return *this;
}
ShaderToyGraph& ShaderToyGraph::setUniform(std::string name, const Color& value) {
    ::core::render::setUniform(*this, std::move(name), ShaderToyUniformKind::Vec4,
                              {value.r, value.g, value.b, value.a});
    return *this;
}
ShaderToyGraph& ShaderToyGraph::setUniform(std::string name, int value) {
    ::core::render::setUniform(*this, std::move(name), ShaderToyUniformKind::Int,
                              {static_cast<float>(value), 0.0f, 0.0f, 0.0f});
    return *this;
}

const ShaderToyPass* ShaderToyGraph::findPass(const std::string& name) const {
    const auto found = std::find_if(passes.begin(), passes.end(),
        [&](const ShaderToyPass& pass) { return pass.name == name; });
    return found == passes.end() ? nullptr : &*found;
}
ShaderToyPass* ShaderToyGraph::findPass(const std::string& name) {
    const auto found = std::find_if(passes.begin(), passes.end(),
        [&](const ShaderToyPass& pass) { return pass.name == name; });
    return found == passes.end() ? nullptr : &*found;
}

bool isShaderToyReservedUniform(const std::string& name) {
    return std::any_of(std::begin(kReservedUniforms), std::end(kReservedUniforms),
        [&](const char* reserved) { return name == reserved; });
}

ShaderToyValidationResult validateShaderToyGraph(const ShaderToyGraph& graph) {
    ShaderToyValidationResult result;
    if (graph.passes.empty()) {
        result.errors.push_back({ShaderToyErrorCode::EmptyGraph, {}, {}, {}, {}, 0,
                                 "A Shadertoy graph requires at least one pass."});
        return result;
    }

    std::unordered_set<std::string> passNames;
    for (const ShaderToyPass& pass : graph.passes) {
        if (pass.name.empty()) {
            result.errors.push_back({ShaderToyErrorCode::EmptyPassName, {}, {}, {}, pass.fragmentPath, 0,
                                     "Pass names must not be empty."});
        } else if (!passNames.insert(pass.name).second) {
            result.errors.push_back({ShaderToyErrorCode::DuplicatePassName, {}, pass.name, {}, pass.fragmentPath, 0,
                                     "Pass names must be unique."});
        }
        if (pass.sourceKind == ShaderToySourceKind::File &&
            pass.fragmentPath.empty()) {
            result.errors.push_back({ShaderToyErrorCode::MissingFragmentPath, {}, pass.name, "fragment", {}, 0,
                                     "A pass requires a fragment source path."});
        } else if (pass.sourceKind == ShaderToySourceKind::Inline &&
                   pass.fragmentSource.empty()) {
            result.errors.push_back({ShaderToyErrorCode::MissingFragmentPath, {}, pass.name, "fragment", pass.fragmentPath, 0,
                                     "An inline pass requires fragment source text."});
        }
    }

    for (const ShaderToyPass& pass : graph.passes) {
        for (const ShaderToyChannel& channel : pass.channels) {
            if (channel.kind == ShaderToyChannelKind::Image && channel.source.empty()) {
                result.errors.push_back({ShaderToyErrorCode::EmptyImagePath, {}, pass.name, {}, {}, 0,
                                         "An image channel requires a source path."});
            } else if (channel.kind == ShaderToyChannelKind::Buffer) {
                if (channel.source == pass.name) {
                    result.errors.push_back({ShaderToyErrorCode::BufferReferencesSelf, {}, pass.name, {}, {}, 0,
                                             "Use ShaderToyChannel::self() for feedback."});
                } else if (graph.findPass(channel.source) == nullptr) {
                    result.errors.push_back({ShaderToyErrorCode::MissingBufferPass, {}, pass.name, {}, {}, 0,
                                             "A buffer channel references an unknown pass: " + channel.source});
                }
            }
        }
    }

    std::unordered_set<std::string> uniformNames;
    if (graph.uniforms.size() > kShaderToyCustomUniformCount) {
        result.errors.push_back({ShaderToyErrorCode::TooManyUniforms, {}, {}, "uniform", {}, 0,
                                 "A Shadertoy graph supports at most 16 custom uniforms."});
    }
    for (const ShaderToyUniform& uniform : graph.uniforms) {
        ShaderToyErrorCode code = ShaderToyErrorCode::None;
        std::string message;
        if (!validIdentifier(uniform.name)) {
            code = ShaderToyErrorCode::InvalidUniformName;
            message = "Custom uniform names must be valid GLSL identifiers.";
        } else if (isShaderToyReservedUniform(uniform.name)) {
            code = ShaderToyErrorCode::ReservedUniformName;
            message = "Custom uniforms cannot replace Shadertoy built-ins.";
        } else if (!uniformNames.insert(uniform.name).second) {
            code = ShaderToyErrorCode::DuplicateUniformName;
            message = "Custom uniform names must be unique.";
        }
        if (code != ShaderToyErrorCode::None) {
            result.errors.push_back({code, {}, {}, "uniform", {}, 0, std::move(message)});
        }
    }
    return result;
}

std::uint64_t shaderToyGraphHash(const ShaderToyGraph& graph) {
    std::uint64_t hash = 1469598103934665603ull;
    for (const ShaderToyPass& pass : graph.passes) {
        hashString(hash, pass.name);
        hashValue(hash, pass.sourceKind);
        hashString(hash, pass.fragmentPath);
        hashString(hash, pass.fragmentSource);
        hashString(hash, pass.spirvPath);
        for (const ShaderToyChannel& channel : pass.channels) {
            hashValue(hash, channel.kind);
            hashString(hash, channel.source);
        }
    }
    for (const ShaderToyUniform& uniform : graph.uniforms) {
        hashString(hash, uniform.name);
        hashValue(hash, uniform.kind);
        hashBytes(hash, uniform.values.data(), uniform.values.size() * sizeof(float));
    }
    return hash;
}

std::uint64_t shaderToyResourceHash(const ShaderToyGraph& graph) {
    std::uint64_t hash = 1469598103934665603ull;
    for (const ShaderToyPass& pass : graph.passes) {
        hashString(hash, pass.name);
        hashValue(hash, pass.sourceKind);
        hashString(hash, pass.fragmentPath);
        hashString(hash, pass.fragmentSource);
        hashString(hash, pass.spirvPath);
        for (const ShaderToyChannel& channel : pass.channels) {
            hashValue(hash, channel.kind);
            hashString(hash, channel.source);
        }
    }
    for (const ShaderToyUniform& uniform : graph.uniforms) {
        hashString(hash, uniform.name);
        hashValue(hash, uniform.kind);
    }
    return hash;
}

std::uint64_t shaderToyErrorHash(const ShaderToyError& error) {
    if (!error) {
        return 0;
    }
    std::uint64_t hash = 1469598103934665603ull;
    hashValue(hash, error.code);
    hashString(hash, error.elementId);
    hashString(hash, error.passName);
    hashString(hash, error.stage);
    hashString(hash, error.sourcePath);
    hashValue(hash, error.line);
    hashString(hash, error.message);
    return hash;
}

std::string shaderToyDefaultSpirvPath(const std::string& fragmentPath) {
    return fragmentPath.empty() ? std::string{} : fragmentPath + ".spv";
}

bool loadShaderToySource(const std::string& path, std::string& source, ShaderToyError& error) {
    std::ifstream input(std::filesystem::u8path(path), std::ios::binary);
    if (!input) {
        error = {ShaderToyErrorCode::SourceReadFailed, {}, {}, "fragment", path, 0,
                 "Unable to read Shadertoy fragment source."};
        source.clear();
        return false;
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    source = contents.str();
    error = {};
    return true;
}

bool resolveShaderToySource(const ShaderToyPass& pass,
                            std::string& source,
                            ShaderToyError& error) {
    if (pass.sourceKind == ShaderToySourceKind::Inline) {
        if (pass.fragmentSource.empty()) {
            source.clear();
            error = {ShaderToyErrorCode::SourceReadFailed, {}, pass.name,
                     "fragment", pass.fragmentPath, 0,
                     "Inline Shadertoy fragment source is empty."};
            return false;
        }
        source = pass.fragmentSource;
        error = {};
        return true;
    }
    if (!loadShaderToySource(pass.fragmentPath, source, error)) {
        error.passName = pass.name;
        return false;
    }
    return true;
}

std::string normalizeShaderToySource(const std::string& source) {
    const std::vector<GlslToken> tokens = tokenizeGlsl(source);
    const std::vector<SourceInsertion> insertions =
        localInitializations(tokens);
    if (insertions.empty()) return source;
    std::string result;
    std::size_t cursor = 0;
    for (const SourceInsertion& insertion : insertions) {
        result.append(source, cursor, insertion.offset - cursor);
        result += insertion.text;
        cursor = insertion.offset;
    }
    result.append(source, cursor, std::string::npos);
    return result;
}

std::string wrapShaderToyOpenGL(const std::string& source,
                                const std::vector<ShaderToyUniform>& uniforms) {
    return std::string(kOpenGLPrelude) + customUniformDeclarations(uniforms) +
           "#line 1\n" + source + kOpenGLPostlude;
}
std::string wrapShaderToyVulkan(const std::string& source,
                                const std::vector<ShaderToyUniform>& uniforms) {
    return std::string(kVulkanPrelude) + vulkanCustomUniformAliases(uniforms) +
           "#line 1\n" +
           normalizeShaderToySource(source) + kVulkanPostlude;
}

} // namespace core::render
