#!/usr/bin/env python3
"""Generate SPIR-V for every pass declared by an EUI Shadertoy graph."""

import argparse
import json
import pathlib
import subprocess
import tempfile


def fail(message):
    raise SystemExit(message)


def resolve_output(relative_dir, output_root, value):
    configured = pathlib.Path(value)
    if configured.is_absolute():
        return configured
    return output_root / relative_dir / configured


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", required=True)
    parser.add_argument("--asset-root", required=True)
    parser.add_argument("--output-root", required=True)
    parser.add_argument("--wrapper", required=True)
    parser.add_argument("--validator", required=True)
    parser.add_argument("--stamp", required=True)
    args = parser.parse_args()

    config_path = pathlib.Path(args.config).resolve()
    asset_root = pathlib.Path(args.asset_root).resolve()
    output_root = pathlib.Path(args.output_root).resolve()
    try:
        relative_dir = config_path.parent.relative_to(asset_root)
    except ValueError:
        fail("Shadertoy config must be located below its asset root")

    try:
        graph = json.loads(config_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        fail("Unable to read Shadertoy config {}: {}".format(config_path, error))
    if not isinstance(graph, dict):
        fail("Shadertoy config {} must be an object".format(config_path))
    if "version" in graph:
        fail("Shadertoy config {} must not contain a version field".format(config_path))
    passes = graph.get("passes")
    if not isinstance(passes, list) or not passes:
        fail("Shadertoy config {} must contain a non-empty passes array".format(config_path))

    wrapper = pathlib.Path(args.wrapper).resolve()
    validator = pathlib.Path(args.validator).resolve()
    stamp = pathlib.Path(args.stamp).resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    stamp.parent.mkdir(parents=True, exist_ok=True)

    uniforms = []
    uniform_declarations = graph.get("uniforms", [])
    if not isinstance(uniform_declarations, list) or len(uniform_declarations) > 16:
        fail("Shadertoy config {} must contain at most 16 uniforms".format(config_path))
    for uniform in uniform_declarations:
        if (not isinstance(uniform, dict) or
                not isinstance(uniform.get("name"), str) or not uniform["name"] or
                uniform.get("type") not in {"float", "int", "vec2", "vec3", "vec4"}):
            fail("Invalid uniform declaration in {}".format(config_path))
        uniforms.append("{}:{}".format(uniform["name"], uniform["type"]))

    with tempfile.TemporaryDirectory(prefix="eui-shadertoy-") as temporary:
        temporary_root = pathlib.Path(temporary)
        for index, shader_pass in enumerate(passes):
            if not isinstance(shader_pass, dict):
                fail("Pass {} in {} must be an object".format(index, config_path))
            if not isinstance(shader_pass.get("name"), str) or not shader_pass["name"]:
                fail("Pass {} in {} requires a non-empty name".format(index, config_path))
            source = shader_pass.get("source")
            inline_source = shader_pass.get("inlineSource")
            if source is not None and not isinstance(source, str):
                fail("Pass {} source must be a string".format(index))
            if inline_source is not None and not isinstance(inline_source, str):
                fail("Pass {} inlineSource must be a string".format(index))
            if bool(source) == bool(inline_source):
                fail("Pass {} must define exactly one of source or inlineSource".format(index))

            if source:
                source_path = (config_path.parent / source).resolve()
                if not source_path.is_file():
                    fail("Missing Shadertoy source: {}".format(source_path))
                source_name = source_path.name
            else:
                source_name = shader_pass.get("sourceName") or "pass{}.inline.frag".format(index)
                if not isinstance(source_name, str):
                    fail("Pass {} sourceName must be a string".format(index))
                source_name_path = pathlib.Path(source_name)
                if source_name_path.is_absolute() or ".." in source_name_path.parts:
                    fail("Pass {} sourceName must stay within the temporary directory".format(index))
                source_path = temporary_root / source_name_path
                source_path.parent.mkdir(parents=True, exist_ok=True)
                source_path.write_text(inline_source, encoding="utf-8")

            spirv_value = shader_pass.get("spirv")
            if spirv_value is not None and not isinstance(spirv_value, str):
                fail("Pass {} spirv must be a string".format(index))
            if not spirv_value:
                if not source:
                    fail("Inline pass {} must define spirv for Vulkan".format(index))
                spirv_value = source + ".spv"
            output_path = resolve_output(relative_dir, output_root, spirv_value)
            output_path.parent.mkdir(parents=True, exist_ok=True)
            wrapped_path = temporary_root / ("{}.wrapped.frag".format(index))
            wrapper_command = [
                str(wrapper), "--input", str(source_path), "--output", str(wrapped_path)
            ]
            for uniform in uniforms:
                wrapper_command.extend(["--uniform", uniform])
            subprocess.run(wrapper_command, check=True)
            subprocess.run([
                str(validator), "-V", "-S", "frag", str(wrapped_path), "-o", str(output_path)
            ], check=True)

    stamp.write_text("generated\n", encoding="ascii")


if __name__ == "__main__":
    main()
