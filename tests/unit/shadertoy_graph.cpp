#include "core/render/shadertoy.h"
#include "eui/types.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <string>
#include <type_traits>

using namespace core::render;

static_assert(std::is_same_v<eui::ShaderToyGraph,
                             core::render::ShaderToyGraph>);

namespace {

ShaderToyGraph validGraph() {
    ShaderToyGraph graph;
    graph.addPass("bufferA", "shaders/buffer_a.frag");
    graph.addPass("image", "shaders/image.frag", "shaders/image.spv");
    graph.setChannel("bufferA", 0, ShaderToyChannel::image("images/noise.png"));
    graph.setChannel("bufferA", 1, ShaderToyChannel::self());
    graph.setChannel("image", 0, ShaderToyChannel::buffer("bufferA"));
    graph.setUniform("uAmount", 0.5f);
    graph.setUniform("uOffset", core::Vec2{1.0f, 2.0f});
    return graph;
}

void validatesGraphContract() {
    ShaderToyGraph graph = validGraph();
    assert(validateShaderToyGraph(graph).valid());
    assert(graph.passes[0].spirvPath == "shaders/buffer_a.frag.spv");
    assert(graph.passes[1].spirvPath == "shaders/image.spv");

    graph.passes[1].name = "bufferA";
    const ShaderToyValidationResult result = validateShaderToyGraph(graph);
    assert(!result.valid());
    assert(result.errors[0].code == ShaderToyErrorCode::DuplicatePassName);
}

void rejectsInvalidChannelsAndUniforms() {
    ShaderToyGraph graph = validGraph();
    graph.setChannel("bufferA", 2, ShaderToyChannel::buffer("bufferA"));
    graph.setChannel("image", 2, ShaderToyChannel::buffer("missing"));
    graph.uniforms.push_back({"iTime", ShaderToyUniformKind::Float, {}});

    const ShaderToyValidationResult result = validateShaderToyGraph(graph);
    bool selfError = false;
    bool missingError = false;
    bool reservedError = false;
    for (const ShaderToyError& error : result.errors) {
        selfError = selfError || error.code == ShaderToyErrorCode::BufferReferencesSelf;
        missingError = missingError || error.code == ShaderToyErrorCode::MissingBufferPass;
        reservedError = reservedError || error.code == ShaderToyErrorCode::ReservedUniformName;
    }
    assert(selfError && missingError && reservedError);
}

void hashesSemanticContent() {
    ShaderToyGraph first = validGraph();
    ShaderToyGraph second = validGraph();
    assert(shaderToyGraphHash(first) == shaderToyGraphHash(second));
    second.setUniform("uAmount", 0.75f);
    assert(shaderToyGraphHash(first) != shaderToyGraphHash(second));
    assert(shaderToyResourceHash(first) == shaderToyResourceHash(second));
    second = validGraph();
    second.setChannel("image", 1, ShaderToyChannel::image("images/other.png"));
    assert(shaderToyGraphHash(first) != shaderToyGraphHash(second));
    assert(shaderToyResourceHash(first) != shaderToyResourceHash(second));
}

void validatesInlineSource() {
    const std::string source =
        "void mainImage(out vec4 color, in vec2 coord) { color = vec4(0.3, 0.4, 0.5, 1.0); }\n";
    ShaderToyGraph graph;
    graph.addInlinePass("image", source, "image.inline.spv",
                        "inline-demo.frag");
    assert(validateShaderToyGraph(graph).valid());
    assert(graph.passes[0].sourceKind == ShaderToySourceKind::Inline);
    assert(graph.passes[0].fragmentPath == "inline-demo.frag");
    std::string resolved;
    ShaderToyError error;
    assert(resolveShaderToySource(graph.passes[0], resolved, error));
    assert(resolved == source && !error);

    graph.passes[0].fragmentSource.clear();
    const ShaderToyValidationResult result = validateShaderToyGraph(graph);
    assert(!result.valid());
    assert(result.errors[0].code ==
           ShaderToyErrorCode::MissingFragmentPath);
}

void parsesGraphJson() {
    const std::string euiJson = R"JSON({
        "passes": [{
            "name": "image",
            "inlineSource": "void mainImage(out vec4 c, in vec2 p) { c = vec4(uTint, 1.0); }",
            "sourceName": "json-inline.frag",
            "spirv": "generated/image.spv",
            "channels": {
                "0": "image:images/input.png"
            }
        }],
        "uniforms": [{
            "name": "uTint",
            "type": "vec3",
            "value": [0.2, 0.4, 0.6]
        }]
    })JSON";
    ShaderToyGraph graph;
    ShaderToyError error;
    assert(parseShaderToyGraphJson(euiJson, "preset", graph, error));
    assert(!error && graph.passes.size() == 1);
    assert(graph.passes[0].sourceKind == ShaderToySourceKind::Inline);
    assert(std::filesystem::path(graph.passes[0].spirvPath).generic_string() ==
           "preset/generated/image.spv");
    const ShaderToyChannel& channel = graph.passes[0].channels[0];
    assert(channel.kind == ShaderToyChannelKind::Image);
    assert(std::filesystem::path(channel.source).generic_string() ==
           "preset/images/input.png");
    assert(graph.uniforms.size() == 1);
    assert(graph.uniforms[0].kind == ShaderToyUniformKind::Vec3);
    assert(graph.uniforms[0].values[1] == 0.4f);

    assert(!parseShaderToyGraphJson(
        R"JSON({"version":2,"passes":[{"name":"image","source":"x.frag"}]})JSON",
        {}, graph, error));
    assert(error.stage == "graph-json");
}

bool parsesCompactGraphJson() {
    const std::string compactJson = R"JSON({
        "passes": [
            {
                "name": "A",
                "source": "A.frag",
                "channels": {
                    "0": "image:noise.png",
                    "2": "self"
                }
            },
            {
                "name": "Image",
                "source": "Image.frag",
                "channels": {
                    "0": "A",
                    "1": "buffer:A"
                }
            }
        ]
    })JSON";

    ShaderToyGraph graph;
    ShaderToyError error;
    if (!parseShaderToyGraphJson(compactJson, "compact", graph, error) ||
        error || graph.passes.size() != 2) {
        return false;
    }
    if (graph.passes[0].name != "A" ||
        graph.passes[0].channels[0].kind != ShaderToyChannelKind::Image ||
        graph.passes[0].channels[2].kind != ShaderToyChannelKind::Self ||
        graph.passes[1].channels[0].kind != ShaderToyChannelKind::Buffer ||
        graph.passes[1].channels[0].source != "A" ||
        graph.passes[1].channels[1].source != "A") {
        return false;
    }
    if (std::filesystem::path(graph.passes[0].channels[0].source).generic_string() !=
        "compact/noise.png") {
        return false;
    }

    const std::string invalid =
        R"JSON({"passes":[{"name":"Image","source":"Image.frag","channels":{"0":"image:"}}]})JSON";
    return !parseShaderToyGraphJson(invalid, {}, graph, error) &&
           error.stage == "graph-json";
}

void wrapsBothBackendsFromOneSource() {
    const std::string source =
        "void mainImage(out vec4 color, in vec2 coord) { color = vec4(coord / iResolution.xy, iTime, 1.0); }\n";
    const ShaderToyGraph graph = validGraph();
    const std::string gl = wrapShaderToyOpenGL(source, graph.uniforms);
    const std::string vk = wrapShaderToyVulkan(source, graph.uniforms);
    assert(gl.find("#version 330 core") != std::string::npos);
    assert(vk.find("#version 450") != std::string::npos);
    assert(gl.find(source) != std::string::npos);
    assert(vk.find(source) != std::string::npos);
    assert(gl.find("mainImage(euiFragColor, fragCoord)") != std::string::npos);
    assert(vk.find("layout(set = 0, binding = 4)") != std::string::npos);
    assert(gl.find("uniform float uAmount;") != std::string::npos);
    assert(vk.find("#define uOffset euiUniforms.customUniforms[1].xy") != std::string::npos);
    assert(vk.find("vec3 iChannelResolution[4]") != std::string::npos);
}

void initializesUndefinedBasicLocalsWithoutChangingLines() {
    const std::string source =
        "#define DECL vec4 ignoredByMacro;\n"
        "struct Item { float member; };\n"
        "void mainImage(out vec4 color, in vec2 coord) {\n"
        "  float distance, initialized = 2.0;\n"
        "  vec4 first, accumulated, sample; // local values\n"
        "  float values[4];\n"
        "  for (int index; index < 1; ++index) { bool flag; }\n"
        "  color = accumulated + vec4(initialized + coord.x);\n"
        "}\n";
    const std::string normalized = normalizeShaderToySource(source);
    assert(normalized.find("#define DECL vec4 ignoredByMacro;") != std::string::npos);
    assert(normalized.find("struct Item { float member; };") != std::string::npos);
    assert(normalized.find("float distance = float(0), initialized = 2.0;") !=
           std::string::npos);
    assert(normalized.find(
        "vec4 first = vec4(0), accumulated = vec4(0), sample = vec4(0);") !=
        std::string::npos);
    assert(normalized.find("float values[4];") != std::string::npos);
    assert(normalized.find("for (int index = int(0);") != std::string::npos);
    assert(normalized.find("bool flag = bool(0);") != std::string::npos);
    assert(std::count(source.begin(), source.end(), '\n') ==
           std::count(normalized.begin(), normalized.end(), '\n'));

    const std::string gl = wrapShaderToyOpenGL(source);
    const std::string vk = wrapShaderToyVulkan(source);
    assert(gl.find("vec4 first, accumulated, sample;") != std::string::npos);
    assert(vk.find(
        "vec4 first = vec4(0), accumulated = vec4(0), sample = vec4(0);") !=
        std::string::npos);
}

void hashesStructuredErrors() {
    ShaderToyError first{ShaderToyErrorCode::ShaderCompileFailed, "toy", "image",
                         "fragment", "image.frag", 7, "syntax error"};
    ShaderToyError second = first;
    assert(shaderToyErrorHash(first) == shaderToyErrorHash(second));
    second.line = 8;
    assert(shaderToyErrorHash(first) != shaderToyErrorHash(second));
    assert(shaderToyErrorHash({}) == 0);
}

} // namespace

int main() {
    validatesGraphContract();
    rejectsInvalidChannelsAndUniforms();
    hashesSemanticContent();
    validatesInlineSource();
    parsesGraphJson();
    if (!parsesCompactGraphJson()) return 1;
    wrapsBothBackendsFromOneSource();
    initializesUndefinedBasicLocalsWithoutChangingLines();
    hashesStructuredErrors();
    return 0;
}
