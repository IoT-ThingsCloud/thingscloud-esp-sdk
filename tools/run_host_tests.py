"""Run MQTT regression tests with mocked transport and real ArduinoJson 6.

Supports GCC/Clang on PATH or Visual Studio 2022 on Windows.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json-library", type=Path,
                        default=ROOT / ".build-tests/sketchbook/libraries/ArduinoJson/src")
    args = parser.parse_args()
    if not (args.json_library / "ArduinoJson.h").is_file():
        raise SystemExit("Install ArduinoJson 6 first (see tests/README.md)")
    build = ROOT / ".build-tests/host"
    build.mkdir(parents=True, exist_ok=True)
    include = [ROOT / "tests/host", ROOT / "src", args.json_library.resolve()]
    defines = ["ESP32", "ARDUINOJSON_ENABLE_ARDUINO_STRING=1",
               "ARDUINOJSON_ENABLE_ARDUINO_STREAM=0", "ARDUINOJSON_ENABLE_ARDUINO_PRINT=0",
               "ARDUINOJSON_ENABLE_PROGMEM=0"]
    sources = [ROOT / "tests/host/mqtt_regression.cpp"]
    env = dict(os.environ)
    compiler = shutil.which("g++") or shutil.which("clang++")
    exe = build / ("mqtt_regression.exe" if os.name == "nt" else "mqtt_regression")
    if compiler:
        command = [compiler, "-std=c++14", "-Wall", "-Wextra", "-g"]
        command += ["-I" + str(path) for path in include] + ["-D" + value for value in defines]
        command += [str(path) for path in sources] + ["-o", str(exe)]
    elif os.name == "nt":
        vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
        vs = subprocess.check_output([str(vswhere), "-latest", "-products", "*",
                                      "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                                      "-property", "installationPath"], text=True).strip()
        vcvars = Path(vs) / "VC/Auxiliary/Build/vcvars64.bat"
        output = subprocess.check_output(f'chcp 65001 >nul && call "{vcvars}" >nul && set',
                                         shell=True, text=True, encoding="utf-8", errors="replace")
        for line in output.splitlines():
            if "=" in line and not line.startswith("="):
                key, value = line.split("=", 1)
                env[key.upper()] = value
        compiler = shutil.which("cl.exe", path=env["PATH"])
        command = [compiler, "/nologo", "/std:c++14", "/EHsc", "/W4", "/fsanitize=address", "/Zi"]
        command += ["/I" + str(path) for path in include] + ["/D" + value for value in defines]
        command += [str(path) for path in sources] + ["/Fe:" + str(exe)]
    else:
        raise SystemExit("A native C++ compiler is required")
    subprocess.run(command, cwd=build, env=env, check=True)
    return subprocess.run([str(exe)], cwd=build, env=env).returncode


if __name__ == "__main__":
    raise SystemExit(main())
