Import("env")

from pathlib import Path
import os
import shutil
import subprocess

def merge_factory(source, target, env):
    project_dir = Path(env.subst("$PROJECT_DIR"))
    build_dir = Path(env.subst("$BUILD_DIR"))
    python_exe = env.subst("$PYTHONEXE")

    bootloader = build_dir / "bootloader.bin"
    partitions = build_dir / "partitions.bin"
    firmware = build_dir / "firmware.bin"

    if not all(p.exists() for p in (bootloader, partitions, firmware)):
        print("[goblin] merge skipped: build products missing")
        return

    package_dir = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32"))
    boot_app0_candidates = [
        package_dir / "tools/partitions/boot_app0.bin",
        package_dir / "tools/partitions/boot_app0.bin",
    ]
    boot_app0 = next((p for p in boot_app0_candidates if p.exists()), None)

    if boot_app0 is None:
        print("[goblin] merge skipped: boot_app0.bin not found")
        return

    out_dir = project_dir / "dist"
    out_dir.mkdir(parents=True, exist_ok=True)
    out_file = out_dir / "ESP-Goblin-ES3C28P-v0.1.0-alpha.0.bin"

    cmd = [
        python_exe, "-m", "esptool",
        "--chip", "esp32s3",
        "merge_bin",
        "-o", str(out_file),
        "--flash_mode", "dio",
        "--flash_freq", "80m",
        "--flash_size", "16MB",
        "0x0000", str(bootloader),
        "0x8000", str(partitions),
        "0xe000", str(boot_app0),
        "0x10000", str(firmware),
    ]

    print("[goblin] merging single-file release image...")
    result = subprocess.run(cmd, cwd=str(project_dir), check=False)

    if result.returncode != 0:
        print(f"[goblin] merge failed with code {result.returncode}")
        return

    print(f"[goblin] factory image: {out_file}")
    print("[goblin] flash this merged image at offset 0x0000")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_factory)
