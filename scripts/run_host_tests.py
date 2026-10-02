"""Build/run all core host tests without ESP32 hardware. Set CXX to a compiler path."""
from pathlib import Path
import os
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
output = root / ".pio" / "host-tests"
output.mkdir(parents=True, exist_ok=True)
environment = os.environ.copy()
environment["ZIG_GLOBAL_CACHE_DIR"] = str(root / ".pio" / "zig-cache")
compiler = os.environ.get("CXX") or shutil.which("c++") or shutil.which("g++")
if compiler:
    command = [compiler]
else:
    python = root / ".pio" / "venv" / "Scripts" / "python.exe"
    if not python.exists():
        sys.exit("Set CXX to a C++11 compiler; no compiler found.")
    command = [str(python), "-m", "ziglang", "c++"]

core = ["src/access_point_inventory.cpp"]
guard = core + ["src/trusted_ap_store.cpp", "src/guard_analyzer.cpp", "src/ble_privacy.cpp", "src/ble_signatures.cpp"]
suites = {
    "ble_privacy_test": guard + ["src/radio_coordinator.cpp"],
    "access_point_inventory_test": core,
    "guard_baseline_test": guard,
    "air_monitor_test": guard + ["src/wifi_frame_event.cpp", "src/deauth_detector.cpp", "src/air_monitor.cpp"],
    "event_log_test": ["src/event_record.cpp", "src/event_logger.cpp", "src/log_policy.cpp", "src/sha256_goblin.cpp"],
}
for name, sources in suites.items():
    executable = output / (name + (".exe" if os.name == "nt" else ""))
    args = command + ["-std=c++11", "-Wall", "-Wextra", "-pthread", "-Iinclude"]
    args += sources + [f"tests/{name}.cpp", "-o", str(executable)]
    with (output / f"{name}.log").open("w") as log:
        result = subprocess.run(args, cwd=root, env=environment, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        print((output / f"{name}.log").read_text(errors="replace"))
        sys.exit(result.returncode)
    subprocess.run([str(executable)], cwd=root, check=True, timeout=60)
print("PASS: all host suites")
