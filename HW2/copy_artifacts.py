"""
Post-build script: Copy build artifacts to Debug/ folder
=========================================================
Copies firmware.bin, firmware.elf to Debug/ directory
for compatibility with CubeIDE structure.
"""

Import("env")
import os
import shutil

# Project directories
PROJECT_DIR = env.get("PROJECT_DIR")
BUILD_DIR = os.path.join(PROJECT_DIR, ".pio", "build", env.get("PIOENV"))
DEBUG_DIR = os.path.join(PROJECT_DIR, "Debug")

def copy_artifacts(source, target, env):
    """Copy build artifacts after build completes"""
    
    # Create Debug directory if not exists
    os.makedirs(DEBUG_DIR, exist_ok=True)
    
    # Files to copy
    files_to_copy = [
        ("firmware.bin", "Binary firmware"),
        ("firmware.elf", "ELF debug file"),
    ]
    
    print(f"\n{'='*60}")
    print(f"Copying build artifacts to Debug/")
    print(f"{'='*60}")
    
    for filename, description in files_to_copy:
        src = os.path.join(BUILD_DIR, filename)
        dst = os.path.join(DEBUG_DIR, filename)
        
        if os.path.exists(src):
            shutil.copy2(src, dst)
            file_size = os.path.getsize(dst)
            print(f"[OK] {filename} ({file_size:,} bytes) - {description}")
        else:
            print(f"[!] {filename} not found")
    
    print(f"{'='*60}\n")

# Add post-build action
env.AddPostAction("$BUILD_DIR/firmware.elf", copy_artifacts)
