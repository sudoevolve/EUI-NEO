#pragma once

#include "core/render/render_types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace core::render {

inline constexpr std::size_t kShaderToyChannelCount = 4;
inline constexpr std::size_t kShaderToyCustomUniformCount = 16;
enum class ShaderToyChannelKind {
    None,
    Image,
    Buffer,
    Self
};
enum class ShaderToyUniformKind { Float, Vec2, Vec3, Vec4, Int };
enum class ShaderToySourceKind { File, Inline };

struct ShaderToyChannel {
    ShaderToyChannelKind kind = ShaderToyChannelKind::None;
    std::string source;

    static ShaderToyChannel none();
    static ShaderToyChannel image(std::string path);
    static ShaderToyChannel buffer(std::string passName);
    static ShaderToyChannel self();
};

struct ShaderToyUniform {
    std::string name;
    ShaderToyUniformKind kind = ShaderToyUniformKind::Float;
    std::array<float, 4> values{};
};

struct ShaderToyPass {
    std::string name;
    ShaderToySourceKind sourceKind = ShaderToySourceKind::File;
    std::string fragmentPath;
    std::string fragmentSource;
    std::string spirvPath;
    std::array<ShaderToyChannel, kShaderToyChannelCount> channels{};
};

struct ShaderToyGraph {
    std::vector<ShaderToyPass> passes;
    std::vector<ShaderToyUniform> uniforms;

    ShaderToyPass& addPass(std::string name, std::string fragmentPath, std::string spirvPath = {});
    ShaderToyPass& addInlinePass(std::string name,
                                std::string fragmentSource,
                                std::string spirvPath,
                                std::string sourceName = {});
    ShaderToyGraph& setChannel(const std::string& passName, std::size_t channel, ShaderToyChannel input);
    ShaderToyGraph& setUniform(std::string name, float value);
    ShaderToyGraph& setUniform(std::string name, const Vec2& value);
    ShaderToyGraph& setUniform(std::string name, const Vec3& value);
    ShaderToyGraph& setUniform(std::string name, const Color& value);
    ShaderToyGraph& setUniform(std::string name, int value);
    const ShaderToyPass* findPass(const std::string& name) const;
    ShaderToyPass* findPass(const std::string& name);
};

enum class ShaderToyErrorCode {
    None,
    EmptyGraph,
    EmptyPassName,
    DuplicatePassName,
    MissingFragmentPath,
    MissingBufferPass,
    BufferReferencesSelf,
    EmptyImagePath,
    InvalidUniformName,
    ReservedUniformName,
    DuplicateUniformName,
    TooManyUniforms,
    SourceReadFailed,
    ShaderCompileFailed,
    ShaderLinkFailed,
    ResourceCreationFailed,
    Unsupported
};

struct ShaderToyError {
    ShaderToyErrorCode code = ShaderToyErrorCode::None;
    std::string elementId;
    std::string passName;
    std::string stage;
    std::string sourcePath;
    int line = 0;
    std::string message;

    explicit operator bool() const { return code != ShaderToyErrorCode::None; }
};

struct ShaderToyValidationResult {
    std::vector<ShaderToyError> errors;
    bool valid() const { return errors.empty(); }
};

struct ShaderToyFrameData {
    float time = 0.0f;
    float deltaTime = 0.0f;
    float frameRate = 0.0f;
    int frame = 0;
    std::array<float, 4> date{};
    std::array<float, 4> mouse{};
    std::array<float, kShaderToyChannelCount> channelTime{};
    float sampleRate = 44100.0f;
    std::uint64_t frameToken = 0;
};

ShaderToyValidationResult validateShaderToyGraph(const ShaderToyGraph& graph);
bool parseShaderToyGraphJson(const std::string& json,
                             const std::string& baseDirectory,
                             ShaderToyGraph& graph,
                             ShaderToyError& error);
bool loadShaderToyGraphJson(const std::string& path,
                            ShaderToyGraph& graph,
                            ShaderToyError& error);
std::uint64_t shaderToyGraphHash(const ShaderToyGraph& graph);
std::uint64_t shaderToyResourceHash(const ShaderToyGraph& graph);
std::uint64_t shaderToyErrorHash(const ShaderToyError& error);
std::string shaderToyDefaultSpirvPath(const std::string& fragmentPath);
bool loadShaderToySource(const std::string& path, std::string& source, ShaderToyError& error);
bool resolveShaderToySource(const ShaderToyPass& pass,
                            std::string& source,
                            ShaderToyError& error);
std::string normalizeShaderToySource(const std::string& source);
std::string wrapShaderToyOpenGL(const std::string& source,
                                const std::vector<ShaderToyUniform>& uniforms = {});
std::string wrapShaderToyVulkan(const std::string& source,
                                const std::vector<ShaderToyUniform>& uniforms = {});
bool isShaderToyReservedUniform(const std::string& name);

} // namespace core::render
