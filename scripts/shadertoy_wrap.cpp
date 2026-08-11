#include "core/render/shadertoy.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {

bool parseUniform(const std::string& specification, core::render::ShaderToyUniform& uniform) {
    const std::size_t separator = specification.find(':');
    if (separator == std::string::npos || separator == 0 || separator + 1 >= specification.size()) {
        return false;
    }
    uniform.name = specification.substr(0, separator);
    const std::string type = specification.substr(separator + 1);
    if (type == "float") uniform.kind = core::render::ShaderToyUniformKind::Float;
    else if (type == "vec2") uniform.kind = core::render::ShaderToyUniformKind::Vec2;
    else if (type == "vec3") uniform.kind = core::render::ShaderToyUniformKind::Vec3;
    else if (type == "vec4") uniform.kind = core::render::ShaderToyUniformKind::Vec4;
    else if (type == "int") uniform.kind = core::render::ShaderToyUniformKind::Int;
    else return false;
    return true;
}

int run(const std::vector<std::string>& arguments) {
    std::string inputPath;
    std::string inputText;
    std::string outputPath;
    std::vector<core::render::ShaderToyUniform> uniforms;
    for (std::size_t index = 1; index < arguments.size(); ++index) {
        const std::string& argument = arguments[index];
        if (argument == "--input" && index + 1 < arguments.size()) {
            inputPath = arguments[++index];
        } else if (argument == "--text" && index + 1 < arguments.size()) {
            inputText = arguments[++index];
        } else if (argument == "--output" && index + 1 < arguments.size()) {
            outputPath = arguments[++index];
        } else if (argument == "--uniform" && index + 1 < arguments.size()) {
            core::render::ShaderToyUniform uniform;
            if (!parseUniform(arguments[++index], uniform)) {
                std::cerr << "Invalid uniform specification. Expected name:type.\n";
                return 2;
            }
            uniforms.push_back(std::move(uniform));
        } else {
            std::cerr << "Unknown or incomplete argument: " << argument << "\n";
            return 2;
        }
    }
    if ((inputPath.empty() == inputText.empty()) || outputPath.empty()) {
        std::cerr << "Usage: eui_shadertoy_wrap (--input shader.frag | --text source)"
                     " --output wrapped.frag"
                     " [--uniform name:type]\n";
        return 2;
    }

    core::render::ShaderToyGraph graph;
    if (inputText.empty()) {
        graph.addPass("image", inputPath);
    } else {
        graph.addInlinePass("image", inputText, "inline.spv");
    }
    graph.uniforms = uniforms;
    const core::render::ShaderToyValidationResult validation =
        core::render::validateShaderToyGraph(graph);
    if (!validation.valid()) {
        std::cerr << validation.errors.front().message << "\n";
        return 3;
    }

    std::string source;
    core::render::ShaderToyError error;
    if (!core::render::resolveShaderToySource(
            graph.passes.front(), source, error)) {
        std::cerr << error.message << ": "
                  << graph.passes.front().fragmentPath << "\n";
        return 4;
    }
    std::ofstream output(std::filesystem::u8path(outputPath),
                         std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "Unable to write wrapped shader: " << outputPath << "\n";
        return 5;
    }
    output << core::render::wrapShaderToyVulkan(source, uniforms);
    return output ? 0 : 5;
}

#ifdef _WIN32
std::string utf8(const wchar_t* value) {
    if (value == nullptr) return {};
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1,
                        result.data(), size, nullptr, nullptr);
    result.pop_back();
    return result;
}
#endif

} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        arguments.push_back(utf8(argv[index]));
    }
    return run(arguments);
}
#else
int main(int argc, char** argv) {
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }
    return run(arguments);
}
#endif
