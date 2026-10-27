"""Compile every example against this checkout, never an installed SDK copy.

Requires Arduino CLI, esp32/esp8266 board cores, and the third-party libraries
listed in tests/README.md. Build products and logs stay in .build-tests/.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import os
from pathlib import Path
import subprocess
import time


ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / ".build-tests"
SHARED_TUTORIALS = {
    "relay_control", "relay_control_advanced", "dht_sensor",
    "bh1750_sensor", "mpu6050_sensor",
}
THIRD_PARTY = [
    "PubSubClient", "DHT_sensor_library", "Adafruit_Unified_Sensor",
    "Adafruit_BusIO", "Adafruit_MPU6050", "BH1750", "ESP32Servo",
]


def matrix():
    for sketch in sorted((ROOT / "examples").rglob("*.ino")):
        name = sketch.stem
        if name.startswith("esp32s3"):
            targets = [("esp32s3", "esp32:esp32:esp32s3:PSRAM=opi")]
        elif name.startswith("esp01_"):
            targets = [("esp8266", "esp8266:esp8266:generic")]
        else:
            targets = [("esp32", "esp32:esp32:esp32")]
            if (sketch.relative_to(ROOT / "examples").parts[0] in {
                "01.WiFi_Basic", "03.MQTT_Communicate"
            } or name in SHARED_TUTORIALS or name == "wifi_provisioning_cliam_device"):
                targets.append(("esp8266", "esp8266:esp8266:generic"))
        for target, fqbn in targets:
            yield sketch, target, fqbn


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--libraries", type=Path, required=True,
                        help="Folder containing third-party Arduino libraries")
    parser.add_argument("--cli", default="arduino-cli")
    parser.add_argument("--jobs", type=int, default=6)
    parser.add_argument("--parallel", type=int, default=1, help="Independent sketch builds to run at once")
    parser.add_argument("--match", default="", help="Optional substring of example/target")
    parser.add_argument("--resume", action="store_true", help="Skip recorded passing builds")
    parser.add_argument("--clean", action="store_true", help="Rebuild without cached compilation products")
    args = parser.parse_args()

    WORK.mkdir(exist_ok=True)
    temp = WORK / "tmp"
    temp.mkdir(exist_ok=True)
    config = WORK / "arduino-cli.yaml"
    # JSON is valid YAML. An isolated sketchbook prevents the IDE's SDK copy or
    # ArduinoJson 7 from entering the dependency search.
    config.write_text(json.dumps({"directories": {
        "user": str(WORK / "sketchbook"), "downloads": str(WORK / "downloads")
    }}, indent=2), encoding="utf-8")
    env = dict(os.environ, TEMP=str(temp), TMP=str(temp))
    base = [args.cli, "--config-file", str(config), "--no-color"]
    subprocess.run(base + ["lib", "install", "ArduinoJson@6.21.5"],
                   check=True, env=env)
    deps = []
    for name in THIRD_PARTY:
        path = args.libraries / name
        if not (path / "library.properties").is_file():
            raise SystemExit(f"Missing third-party library: {path}")
        deps.extend(["--library", str(path.resolve())])

    results_path = WORK / "results.json"
    previous = json.loads(results_path.read_text()) if args.resume and results_path.exists() else []
    results = list(previous)
    passed = {item["id"] for item in previous if item["passed"]}
    builds = list(matrix())
    selected_ids = {f"{sketch.stem}--{target}" for sketch, target, _ in builds
                    if args.match in f"{sketch.stem}--{target}"}
    if not selected_ids:
        raise SystemExit(f"No examples match: {args.match}")

    def compile_one(index, sketch, target, fqbn):
        identity = f"{sketch.stem}--{target}"
        log = WORK / "logs" / f"{identity}.log"
        log.parent.mkdir(exist_ok=True, parents=True)
        build = WORK / "build" / identity
        command = base + ["compile", "--fqbn", fqbn, "--jobs", str(args.jobs),
                          "--library", str(ROOT), *deps,
                          "--build-path", str(build), str(sketch.parent)]
        if args.clean:
            command.append("--clean")
        print(f"[{index}/{len(builds)}] {identity}", flush=True)
        started = time.monotonic()
        with log.open("w", encoding="utf-8") as stream:
            process = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, env=env)
        output = log.read_text(encoding="utf-8", errors="replace")
        # Arduino's build metadata records the source locations that were compiled.
        metadata = build / "libraries.cache"
        sdk_sources = []
        if metadata.exists():
            include_paths = json.loads(metadata.read_text())
            sdk_sources = [str(Path(path).resolve()) for path in include_paths
                           if (Path(path) / "ThingsCloudMQTT.cpp").is_file()]
        source_verified = sdk_sources == [str(ROOT / "src")]
        item = {"id": identity, "example": str(sketch.relative_to(ROOT)), "fqbn": fqbn,
                "passed": process.returncode == 0 and source_verified,
                "exit_code": process.returncode, "sdk_sources": sdk_sources,
                "source_verified": source_verified,
                "seconds": round(time.monotonic() - started, 1),
                "log": str(log.relative_to(ROOT))}
        print(f"  {'PASS' if item['passed'] else 'FAIL'} {identity} ({item['seconds']}s)", flush=True)
        if not item["passed"]:
            print(output[-6000:], flush=True)
            if not source_verified:
                print(f"SDK source verification failed: {sdk_sources}", flush=True)
        return item

    with ThreadPoolExecutor(max_workers=args.parallel) as executor:
        pending = []
        for index, (sketch, target, fqbn) in enumerate(builds, 1):
            identity = f"{sketch.stem}--{target}"
            if args.match in identity and not (args.resume and identity in passed):
                pending.append(executor.submit(compile_one, index, sketch, target, fqbn))
        for future in as_completed(pending):
            item = future.result()
            results = [old for old in results if old["id"] != item["id"]] + [item]
            results_path.write_text(json.dumps(results, indent=2, ensure_ascii=False), encoding="utf-8")

    selected = [item for item in results if item["id"] in selected_ids]
    failures = [item for item in selected if not item["passed"]]
    missing = selected_ids - {item["id"] for item in selected}
    print(f"Results: {len(selected) - len(failures)}/{len(selected_ids)} passed; {results_path}", flush=True)
    if missing:
        print(f"Missing results: {sorted(missing)}", flush=True)
    return bool(failures or missing)


if __name__ == "__main__":
    raise SystemExit(main())
