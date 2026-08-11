#!/usr/bin/env python3

import argparse
import pathlib
import shutil
import subprocess
import sys
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
SHADER_DIR = ROOT / "core" / "render" / "vulkan" / "shaders"
OUT_DIR = ROOT / "core" / "render" / "vulkan"

GROUPS = [
    {
        "name": "ShaderToy",
        "comment": "shadertoy.vert",
        "header": "vulkan_shadertoy_shaders.h",
        "stages": [
            ("Vertex", "shadertoy.vert"),
        ],
    },
    {
        "name": "RoundedRect",
        "comment": "rounded_rect*.vert and rounded_rect*.frag",
        "header": "vulkan_rounded_rect_shaders.h",
        "stages": [
            ("Vertex", "rounded_rect.vert"),
            ("Fragment", "rounded_rect.frag"),
            ("BatchVertex", "rounded_rect_batch.vert"),
            ("BatchFragment", "rounded_rect_batch.frag"),
        ],
    },
    {
        "name": "Text",
        "comment": "text.vert and text.frag",
        "header": "vulkan_text_shaders.h",
        "stages": [
            ("Vertex", "text.vert"),
            ("Fragment", "text.frag"),
        ],
    },
    {
        "name": "Polygon",
        "comment": "polygon.vert and polygon.frag",
        "header": "vulkan_polygon_shaders.h",
        "stages": [
            ("Vertex", "polygon.vert"),
            ("Fragment", "polygon.frag"),
        ],
    },
    {
        "name": "Image",
        "comment": "image.vert and image.frag",
        "header": "vulkan_image_shaders.h",
        "stages": [
            ("Vertex", "image.vert"),
            ("Fragment", "image.frag"),
        ],
    },
    {
        "name": "RenderCacheResolve",
        "comment": "render_cache_resolve.vert and render_cache_resolve.frag",
        "header": "vulkan_render_cache_resolve_shaders.h",
        "stages": [
            ("Vertex", "render_cache_resolve.vert"),
            ("Fragment", "render_cache_resolve.frag"),
        ],
    },
]


def words_from_spirv(data: bytes) -> list[int]:
    if len(data) % 4 != 0:
        raise RuntimeError("SPIR-V bytecode size is not 4-byte aligned")
    return [int.from_bytes(data[i:i + 4], "little") for i in range(0, len(data), 4)]


def format_words(words: list[int]) -> str:
    lines = []
    for index in range(0, len(words), 8):
        chunk = words[index:index + 8]
        lines.append("    " + ", ".join(f"0x{word:08x}u" for word in chunk) + ",")
    return "\n".join(lines)


def compile_shader(glslang: str, source: pathlib.Path, output: pathlib.Path) -> None:
    subprocess.run(
        [glslang, "-V", str(source), "-o", str(output)],
        cwd=ROOT,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )


def generate_header(group: dict, glslang: str, temp_dir: pathlib.Path) -> str:
    sections = []
    for label, filename in group["stages"]:
        source = SHADER_DIR / filename
        spv = temp_dir / f"{filename}.spv"
        compile_shader(glslang, source, spv)
        words = words_from_spirv(spv.read_bytes())
        symbol = f"k{group['name']}{label}Spirv"
        sections.append(
            f"inline constexpr std::uint32_t {symbol}[] = {{\n"
            f"{format_words(words)}\n"
            f"}};\n"
            f"inline constexpr std::size_t {symbol}Size = sizeof({symbol});"
        )

    return (
        "#pragma once\n\n"
        "#include <cstddef>\n"
        "#include <cstdint>\n\n"
        "namespace core::render::vulkan::shaders {\n\n"
        f"// Generated from core/render/vulkan/shaders/{group['comment']}.\n"
        "// Regenerate with scripts/generate_vulkan_shaders.py when changing shader source.\n\n"
        + "\n\n".join(sections) +
        "\n\n} // namespace core::render::vulkan::shaders\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate Vulkan SPIR-V header files from GLSL sources.")
    parser.add_argument("--check", action="store_true", help="Fail if generated headers differ from checked-in files.")
    args = parser.parse_args()

    glslang = shutil.which("glslangValidator")
    if glslang is None:
        print("glslangValidator was not found in PATH.", file=sys.stderr)
        return 2

    changed = []
    with tempfile.TemporaryDirectory() as temp:
        temp_dir = pathlib.Path(temp)
        for group in GROUPS:
            generated = generate_header(group, glslang, temp_dir)
            target = OUT_DIR / group["header"]
            current = target.read_text() if target.exists() else ""
            if generated != current:
                changed.append(target)
                if not args.check:
                    target.write_text(generated)

    if changed and args.check:
        for path in changed:
            print(f"out of date: {path.relative_to(ROOT)}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
