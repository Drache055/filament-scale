# PlatformIO-Pre-Skript: bettet die Firmware-Version als FW_VERSION ein.
# Reihenfolge: Umgebungsvariable FW_VERSION (Release-Workflow setzt den Tag), sonst `git describe`, sonst "dev".
import os
import subprocess

Import("env")  # noqa: F821 (von PlatformIO bereitgestellt)

version = os.environ.get("FW_VERSION", "").strip()
if not version:
    try:
        version = subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"],
            cwd=env.subst("$PROJECT_DIR"),  # noqa: F821
            stderr=subprocess.DEVNULL,
        ).decode().strip()
    except Exception:
        version = ""
version = "".join(c for c in version if c.isalnum() or c in ".-_+")[:31] or "dev"
print("Firmware-Version: " + version)
env.Append(CPPDEFINES=[("FW_VERSION", env.StringifyMacro(version))])  # noqa: F821
