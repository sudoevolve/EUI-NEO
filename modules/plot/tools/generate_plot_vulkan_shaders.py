"""Compile the module's Vulkan shaders into its checked-in SPIR-V header."""

import argparse
import pathlib
import shutil
import subprocess
import sys
import tempfile


MODULE = pathlib.Path(__file__).resolve().parents[1]
SHADERS = MODULE / "shaders"
OUTPUT = MODULE / "vulkan_plot_shaders.h"
STAGES = (("kPlotVertexSpirv", "plot.vert"), ("kPlotFragmentSpirv", "plot.frag"))


def format_words(data: bytes) -> str:
    if len(data) % 4:
        raise RuntimeError("SPIR-V bytecode size is not 4-byte aligned")
    words = [int.from_bytes(data[i : i + 4], "little") for i in range(0, len(data), 4)]
    return "\n".join(
        "    " + ", ".join(f"0x{word:08x}u" for word in words[i : i + 8]) + ","
        for i in range(0, len(words), 8)
    )


def generate(compiler: str) -> str:
    sections = []
    with tempfile.TemporaryDirectory(prefix="eui-plot-shaders-") as temporary:
        temporary_path = pathlib.Path(temporary)
        for symbol, filename in STAGES:
            source = SHADERS / filename
            binary = temporary_path / f"{filename}.spv"
            subprocess.run([compiler, "-V", str(source), "-o", str(binary)], check=True)
            sections.append(
                f"inline constexpr std::uint32_t {symbol}[] = {{\n"
                f"{format_words(binary.read_bytes())}\n"
                f"}};\ninline constexpr std::size_t {symbol}Size = sizeof({symbol});"
            )
    return (
        "#pragma once\n\n"
        "#include <cstddef>\n"
        "#include <cstdint>\n\n"
        "namespace modules::plot::detail {\n\n"
        "// Generated from shaders/plot.vert and shaders/plot.frag.\n"
        "// Regenerate with tools/generate_plot_vulkan_shaders.py.\n\n"
        + "\n\n".join(sections)
        + "\n\n} // namespace modules::plot::detail\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Fail if the checked-in header is out of date.")
    args = parser.parse_args()
    compiler = shutil.which("glslangValidator")
    if compiler is None:
        print("glslangValidator was not found in PATH.", file=sys.stderr)
        return 2
    generated = generate(compiler)
    current = OUTPUT.read_text(encoding="utf-8") if OUTPUT.exists() else ""
    if args.check:
        if generated != current:
            print(f"out of date: {OUTPUT.relative_to(MODULE.parent)}", file=sys.stderr)
            return 1
        return 0
    with OUTPUT.open("w", encoding="utf-8", newline="\n") as output:
        output.write(generated)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
