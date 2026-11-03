#!/usr/bin/env python3
"""Run the plot module's Vulkan release gate with CMake, CTest, and Python only."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
from datetime import datetime, timezone


MODULE = Path(__file__).resolve().parents[1]
REPO = MODULE.parents[1]


def run(log_dir: Path, name: str, command: list[str], env: dict[str, str] | None = None) -> str:
    print("+", subprocess.list2cmdline(command), flush=True)
    result = subprocess.run(command, cwd=REPO, env=env, text=True, encoding="utf-8", errors="replace",
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
    output = result.stdout
    (log_dir / name).write_text(output, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}); see {log_dir / name}\n{output[-4000:]}")
    return output


def require_clean_validation(output: str, label: str) -> None:
    markers = ("Validation Error", "VUID-", "SYNC-HAZARD")
    if any(marker in output for marker in markers):
        raise RuntimeError(f"Vulkan validation diagnostics found in {label}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=REPO / "build-plot-vulkan-release")
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--probe-repeats", type=int, default=5)
    parser.add_argument("--parallel", type=int, default=max(1, min(8, os.cpu_count() or 1)))
    args = parser.parse_args()
    if not 1 <= args.probe_repeats <= 100 or args.parallel < 1:
        parser.error("probe-repeats must be 1..100 and parallel must be positive")

    build = args.build_dir.resolve()
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    output = (args.output_dir or REPO / "temp" / f"plot-vulkan-gate-{stamp}").resolve()
    if output.exists():
        raise RuntimeError(f"Output directory must be new: {output}")
    output.mkdir(parents=True)

    vulkaninfo = shutil.which("vulkaninfo")
    if not vulkaninfo:
        raise RuntimeError("vulkaninfo is required to verify the Vulkan validation layer")
    summary = run(output, "vulkaninfo.log", [vulkaninfo, "--summary"])
    if "VK_LAYER_KHRONOS_validation" not in summary:
        raise RuntimeError("VK_LAYER_KHRONOS_validation is unavailable; this is not a passing gate")

    configure = ["cmake", "-S", str(REPO), "-B", str(build),
                 "-DEUI_DEPS_MODE=bundled", "-DEUI_BUILD_APPS=OFF", "-DEUI_BUILD_USER_APPS=OFF",
                 "-DEUI_BUILD_TEST_FIXTURES=ON", "-DEUI_ENABLE_INSTALL=ON", "-DEUI_BUILD_SHARED=OFF",
                 "-DEUI_ENABLE_MODULES=ON", "-DEUI_ENABLE_PLOT=ON", "-DEUI_INSTALL_DEMO_ASSETS=OFF",
                 "-DEUI_RENDER_BACKEND=vulkan", "-DEUI_WINDOW_BACKEND=glfw"]
    run(output, "configure.log", configure)

    units = sorted(path.stem for path in (MODULE / "tests" / "unit").glob("plot_*.cpp"))
    targets = ["eui_neo_module_plot", "eui_neo_module_serial", "eui_shadertoy_wrap",
               "plot_vulkan_probe", "plot_interaction_benchmark", "gpu_image_probe", *units]
    run(output, "build.log", ["cmake", "--build", str(build), "--config", "Release", "--parallel",
                              str(args.parallel), "--target", *targets])

    env = os.environ.copy()
    env["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    env["VK_LAYER_VALIDATE_SYNC"] = "1"
    inventory_text = run(output, "test-inventory.json",
                         ["ctest", "--test-dir", str(build), "-C", "Release", "--show-only=json-v1"], env)
    inventory = json.loads(inventory_text)
    registered = {test["name"] for test in inventory.get("tests", [])}
    required = {*units, "plot_vulkan_probe", "plot_interaction_benchmark",
                "plot_interaction_benchmark_125k", "gpu_image_probe"}
    missing = required - registered
    if missing:
        raise RuntimeError(f"Required tests are not registered: {', '.join(sorted(missing))}")

    tests = run(output, "ctest.log", ["ctest", "--test-dir", str(build), "-C", "Release", "-R",
                                      "^(plot_.*|gpu_image_probe)$", "-V", "--no-tests=error"], env)
    repeated = run(output, "probe-repeat.log", ["ctest", "--test-dir", str(build), "-C", "Release", "-R",
                                                 "^(plot_vulkan_probe|gpu_image_probe)$", "--repeat",
                                                 f"until-fail:{args.probe_repeats}", "-V", "--no-tests=error"], env)
    require_clean_validation(tests, "ctest.log")
    require_clean_validation(repeated, "probe-repeat.log")

    benchmark = build / ("Release/plot_interaction_benchmark.exe" if os.name == "nt"
                         else "plot_interaction_benchmark")
    volume = run(output, "interaction-volume.log", [str(benchmark), "--volume"], env)
    require_clean_validation(volume, "interaction-volume.log")

    sdk = output / "sdk"
    run(output, "install.log", ["cmake", "--install", str(build), "--config", "Release", "--prefix", str(sdk)])
    if (sdk / "share/eui-neo/assets").exists():
        raise RuntimeError("Demo assets leaked into the SDK")
    private_headers = ("numeric.h", "colormaps.h", "data_storage.h", "gpu_scene3d_shader.h", "software_raster.h")
    for header in private_headers:
        if (sdk / "include/eui-neo/modules/plot" / header).exists():
            raise RuntimeError(f"Private header leaked: {header}")
    export_dir = sdk / "lib/cmake/EuiNeo"
    source_paths = (str(REPO), REPO.as_posix())
    for export in export_dir.glob("*.cmake"):
        content = export.read_text(encoding="utf-8", errors="replace")
        if any(source_path in content for source_path in source_paths):
            raise RuntimeError(f"Source path leaked into {export.name}")

    consumer = output / "consumer"
    example = sdk / "share/eui-neo/modules/plot/examples/plot_binary_sdk"
    cmake_args = ["-DCMAKE_PREFIX_PATH=" + str(sdk), "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
                  "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF"]
    run(output, "consumer-configure.log", ["cmake", "-S", str(example), "-B", str(consumer), *cmake_args])
    run(output, "consumer-build.log", ["cmake", "--build", str(consumer), "--config", "Release",
                                        "--parallel", str(args.parallel)])
    run(output, "consumer-test.log", ["ctest", "--test-dir", str(consumer), "-C", "Release",
                                      "--output-on-failure", "--no-tests=error"])
    api_consumer = output / "api-consumer"
    run(output, "api-consumer-configure.log", ["cmake", "-S", str(MODULE / "tests/consumers/find-package"),
                                                 "-B", str(api_consumer), *cmake_args])
    run(output, "api-consumer-build.log", ["cmake", "--build", str(api_consumer), "--config", "Release",
                                            "--parallel", str(args.parallel)])
    run(output, "api-consumer-test.log", ["ctest", "--test-dir", str(api_consumer), "-C", "Release",
                                          "--output-on-failure", "--no-tests=error"])

    manifest = []
    for path in sorted(item for item in sdk.rglob("*") if item.is_file()):
        manifest.append({"path": path.relative_to(sdk).as_posix(), "bytes": path.stat().st_size,
                         "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    (output / "sdk-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    status = subprocess.run(["git", "status", "--porcelain"], cwd=REPO, text=True,
                            stdout=subprocess.PIPE, check=True).stdout.splitlines()
    head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=REPO, text=True,
                          stdout=subprocess.PIPE, check=True).stdout.strip()
    result = {"result": "technical-gate-passed", "utc": datetime.now(timezone.utc).isoformat(),
              "base_commit": head, "dirty_worktree": bool(status), "worktree_changes": status,
              "backend": "Vulkan GPU 2D rasterization; CPU geometry",
              "three_dimensional": "CPU reference rasterization", "platform": platform.platform(),
              "validation": "VK_LAYER_KHRONOS_validation + synchronization",
              "probe_repeats": args.probe_repeats,
              "consumer": "binary SDK and find-package consumers built and tested",
              "sales_approval": "Not granted; review licensing, support matrix, and performance scope"}
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Technical gate passed. Evidence: {output}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
