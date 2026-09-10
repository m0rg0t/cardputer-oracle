Import("env")
from pathlib import Path

def check_size(source, target, env):
    size = Path(str(target[0])).stat().st_size
    limit = 0x140000  # Conservative ceiling from the existing Hermes M5Apps layout.
    print(f"Oracle application: {size:,} / {limit:,} bytes")
    if size > limit:
        raise RuntimeError("Application exceeds the reference M5Apps slot")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", check_size)
