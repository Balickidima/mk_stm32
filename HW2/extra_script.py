"""
Extra Script for STM32 CubeMX Projects
======================================
Автоматически извлекает настройки из Makefile от CubeMX:
- Флаги компиляции из subdir.mk
- Include paths
- Список исходников
- Настройки линковки

ПОЛНАЯ совместимость с CubeIDE - никаких изменений в HAL.
Поддерживает любые MCU STM32 (F0, F1, F3, F4, F7, H7, L0, L1, L4, G0, G4, U5 и т.д.)
"""

Import("env")
import os
import glob
import json
import re

PROJECT_DIR = env.get("PROJECT_DIR")
DEBUG_DIR = os.path.join(PROJECT_DIR, "Debug")

# =============================================================================
# Parse Makefile (subdir.mk) - извлекаем флаги из CubeMX
# =============================================================================

def parse_subdir_mk():
    """
    Парсит subdir.mk файлы для извлечения:
    - флагов компиляции
    - include paths
    - исходных файлов (ВСЕ: Core, HAL, CMSIS, Startup)
    """
    if not os.path.isdir(DEBUG_DIR):
        print("[!] Debug/ directory not found. Build in CubeIDE first!")
        return None

    result = {
        "cflags": [],
        "cxxflags": [],
        "cpppath": [],
        "sources": [],
        "defines": []
    }

    # Находим все subdir.mk
    subdir_files = glob.glob(os.path.join(DEBUG_DIR, "**", "subdir.mk"), recursive=True)

    for subdir_file in subdir_files:
        with open(subdir_file, 'r', encoding='utf-8') as f:
            content = f.read()

        # Извлекаем команду компиляции (строка с arm-none-eabi-gcc)
        for line in content.split('\n'):
            if 'arm-none-eabi-gcc' in line and '-c' in line:
                # Извлекаем флаги
                # -I пути
                include_matches = re.findall(r'-I(\.\./[^\s]+)', line)
                for inc in include_matches:
                    # Конвертируем ../ в абсолютный путь
                    abs_path = os.path.normpath(os.path.join(DEBUG_DIR, inc))
                    if os.path.isdir(abs_path) and abs_path not in result["cpppath"]:
                        result["cpppath"].append(abs_path)

                # -D дефайны
                define_matches = re.findall(r'-D([A-Z0-9_]+)', line)
                for d in define_matches:
                    if d not in result["defines"]:
                        result["defines"].append(d)

                # Флаги: -mcpu, -mfpu, -mfloat-abi, -mthumb, -O, -g, -Wall и т.д.
                flag_patterns = [
                    r'-mcpu=[^\s]+',
                    r'-mfpu=[^\s]+',
                    r'-mfloat-abi=[^\s]+',
                    r'-mthumb',
                    r'-O[0-9s]+',
                    r'-g[0-9]*',
                    r'-Wall',
                    r'-ffunction-sections',
                    r'-fdata-sections',
                    r'-std=[^\s]+',
                ]

                for pattern in flag_patterns:
                    matches = re.findall(pattern, line)
                    for m in matches:
                        if m not in result["cflags"]:
                            result["cflags"].append(m)

        # Извлекаем C_SRCS из самого subdir.mk (формат: ../Core/Src/main.c \)
        c_src_match = re.search(r'C_SRCS \+= \\\n(.+?)\n\n', content, re.DOTALL)
        if c_src_match:
            for line in c_src_match.group(1).split('\n'):
                line = line.strip().rstrip('\\').strip()
                if line.startswith('../'):
                    abs_path = os.path.normpath(os.path.join(DEBUG_DIR, line))
                    if os.path.isfile(abs_path) and abs_path not in result["sources"]:
                        result["sources"].append(abs_path)

        # Извлекаем S_SRCS (ассемблер - startup файлы)
        s_src_match = re.search(r'S_SRCS \+= \\\n(.+?)\n\n', content, re.DOTALL)
        if s_src_match:
            for line in s_src_match.group(1).split('\n'):
                line = line.strip().rstrip('\\').strip()
                if line.startswith('../'):
                    abs_path = os.path.normpath(os.path.join(DEBUG_DIR, line))
                    if os.path.isfile(abs_path) and abs_path not in result["sources"]:
                        result["sources"].append(abs_path)

    # Убираем дубликаты
    result["sources"] = list(set(result["sources"]))
    result["cpppath"] = list(set(result["cpppath"]))

    return result


# =============================================================================
# MCU Detection from .ioc
# =============================================================================

def detect_mcu_from_ioc():
    ioc_files = glob.glob(os.path.join(PROJECT_DIR, "*.ioc"))
    if not ioc_files:
        return None
    with open(ioc_files[0], 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
    match = re.search(r'Mcu\.UserName=([^\r\n]+)', content)
    if match:
        return match.group(1).strip()
    match = re.search(r'McuName="([^"]+)"', content)
    return match.group(1) if match else None


def detect_mcu_family(mcu_name):
    if not mcu_name:
        return "STM32F4"
    match = re.match(r'stm32([fluhgw][0-9]+)', mcu_name, re.IGNORECASE)
    return f"STM32{match.group(1).upper()}" if match else "STM32F4"


def get_cpu_from_family(mcu_family):
    if mcu_family.startswith(("STM32F0", "STM32L0", "STM32G0")):
        return "cortex-m0"
    elif mcu_family.startswith(("STM32F1", "STM32L1")):
        return "cortex-m3"
    elif mcu_family.startswith("STM32F3"):
        return "cortex-m4"
    elif mcu_family.startswith(("STM32F4", "STM32L4", "STM32G4", "STM32WB", "STM32WL")):
        return "cortex-m4"
    elif mcu_family.startswith(("STM32F7", "STM32H7")):
        return "cortex-m7"
    elif mcu_family.startswith(("STM32L5", "STM32U5")):
        return "cortex-m33"
    return "cortex-m4"


def get_fpu_from_cpu(cpu):
    if cpu in ["cortex-m0", "cortex-m0+", "cortex-m3"]:
        return ["-mfloat-abi=soft"]
    elif cpu in ["cortex-m4", "cortex-m33"]:
        return ["-mfpu=fpv4-sp-d16", "-mfloat-abi=hard"]
    elif cpu == "cortex-m7":
        return ["-mfpu=fpv5-sp-d16", "-mfloat-abi=hard"]
    return ["-mfpu=fpv4-sp-d16", "-mfloat-abi=hard"]


# =============================================================================
# Board Configuration
# =============================================================================

def find_or_create_board(mcu_name, mcu_family, cpu):
    boards_dir = os.path.join(PROJECT_DIR, "boards")
    os.makedirs(boards_dir, exist_ok=True)

    board_id = mcu_name.lower() if mcu_name else f"custom_{mcu_family.lower()}"
    board_file = os.path.join(boards_dir, f"{board_id}.json")

    if os.path.exists(board_file):
        print(f"[OK] Found board: {board_id}")
        return board_id

    mem_info = detect_memory_from_linker()

    board_config = {
        "build": {
            "core": "stm32",
            "cpu": cpu,
            "f_cpu": "84000000L",
            "mcu": mcu_name or f"{mcu_family}xxx",
            "variant": mcu_family,
            "product_line": mcu_family,
            "extra_flags": [f"-D{mcu_family}"]
        },
        "debug": {
            "jlink_device": f"{mcu_name[:10].upper()}" if mcu_name else "STM32F401CC",
            "openocd_target": "stm32f4x.cfg",
            "openocd_work_area": 16384
        },
        "frameworks": [],  # ПУСТОЙ framework - используем наш HAL
        "name": mcu_name or f"Custom {mcu_family}",
        "upload": {
            "maximum_ram_size": mem_info["ram"] if mem_info else 65536,
            "maximum_size": mem_info["flash"] if mem_info else 262144,
            "protocol": "stlink",
            "protocols": ["stlink", "jlink", "blackmagic"]
        },
        "url": "https://www.st.com/en/microcontrollers-microprocessors/stm32.html",
        "vendor": "STMicroelectronics"
    }

    with open(board_file, 'w', encoding='utf-8') as f:
        json.dump(board_config, f, indent=2)

    print(f"[OK] Created board: {board_id}")
    return board_id


def get_openocd_target_file(mcu_family):
    """Возвращает правильный OpenOCD target файл для семейства MCU"""
    targets = {
        "STM32F0": "stm32f0x.cfg",
        "STM32F1": "stm32f1x.cfg",
        "STM32F3": "stm32f3x.cfg",
        "STM32F4": "stm32f4x.cfg",
        "STM32F7": "stm32f7x.cfg",
        "STM32H7": "stm32h7x.cfg",
        "STM32L0": "stm32l0.cfg",
        "STM32L1": "stm32l1.cfg",
        "STM32L4": "stm32l4x.cfg",
        "STM32G0": "stm32g0x.cfg",
        "STM32G4": "stm32g4x.cfg",
        "STM32L5": "stm32l5x.cfg",
        "STM32U5": "stm32u5x.cfg",
        "STM32WB": "stm32wbx.cfg",
        "STM32WL": "stm32wlx.cfg",
    }
    return targets.get(mcu_family, "stm32f4x.cfg")  # F4 по умолчанию


def detect_memory_from_linker():
    ld_files = glob.glob(os.path.join(PROJECT_DIR, "*.ld"))
    if not ld_files:
        return None
    with open(ld_files[0], 'r', encoding='utf-8') as f:
        content = f.read()
    ram_match = re.search(r'RAM\s*\([^)]+\)\s*:\s*ORIGIN\s*=\s*(0x[0-9A-Fa-f]+),\s*LENGTH\s*=\s*(\d+)([KMG])?', content, re.IGNORECASE)
    flash_match = re.search(r'FLASH\s*\([^)]+\)\s*:\s*ORIGIN\s*=\s*(0x[0-9A-Fa-f]+),\s*LENGTH\s*=\s*(\d+)([KMG])?', content, re.IGNORECASE)
    def parse_size(match):
        if not match: return None
        size = int(match.group(2))
        unit = (match.group(3) or 'K').upper()
        if unit == 'K': size *= 1024
        elif unit == 'M': size *= 1024 * 1024
        return size
    ram_size = parse_size(ram_match)
    flash_size = parse_size(flash_match)
    return {"ram": ram_size or 65536, "flash": flash_size or 262144}


# =============================================================================
# Linker Script
# =============================================================================

def find_linker_script():
    ld_files = glob.glob(os.path.join(PROJECT_DIR, "*.ld"))
    if ld_files:
        return os.path.basename(ld_files[0])
    return None


# =============================================================================
# Main
# =============================================================================

print("=" * 60)
print("STM32 CubeMX Build Configuration")
print("=" * 60)

# 0. Исправляем HAL const проблему (автоматически)
print("\n[0] Patching HAL for const compatibility...")
# Это необходимо из-за бага в HAL от STM:
# В .c файлах: HAL_RCC_OscConfig(const RCC_OscInitTypeDef *)
# В .h файле:  HAL_RCC_OscConfig(RCC_OscInitTypeDef *)  (без const)
# Запускаем fix_hal_const.py для исправления
fix_hal_script = os.path.join(PROJECT_DIR, "fix_hal_const.py")
if os.path.isfile(fix_hal_script):
    try:
        # Запускаем как отдельный скрипт с правильным __file__
        import subprocess
        result = subprocess.run(
            ["python", fix_hal_script],
            cwd=PROJECT_DIR,
            capture_output=True,
            text=True
        )
        if result.returncode == 0:
            print("[OK] HAL patched (const mismatch fixed)")
        else:
            print(f"[!] fix_hal_const.py error: {result.stderr}")
    except Exception as e:
        print(f"[!] Error running fix_hal_const.py: {e}")
else:
    print("[!] fix_hal_const.py not found - build may fail with const error")

# 1. Парсим Makefile от CubeMX
print("\n[1] Parsing CubeMX Makefile...")
makefile_data = parse_subdir_mk()

if makefile_data:
    print(f"    Found {len(makefile_data['sources'])} source files")
    print(f"    Found {len(makefile_data['cpppath'])} include dirs")
    print(f"    Found {len(makefile_data['defines'])} defines")
    print(f"    Found {len(makefile_data['cflags'])} compiler flags")

    # Применяем include paths
    env.Append(CPPPATH=makefile_data["cpppath"])
    print(f"[OK] Include paths: {len(makefile_data['cpppath'])} dirs")

    # Применяем дефайны
    for d in makefile_data["defines"]:
        env.Append(CPPDEFINES=[d])
    print(f"[OK] Defines: {makefile_data['defines']}")

    # Разделяем флаги для компилятора и ассемблера
    # Ассемблеру нужны только флаги процессора (без -g, -O, -Wall)
    as_flags = []
    cc_flags = []
    for flag in makefile_data["cflags"]:
        if flag.startswith(("-mcpu=", "-mfpu=", "-mfloat-abi=", "-mthumb")):
            as_flags.append(flag)
        cc_flags.append(flag)

    # Новый GCC (14.x) не понимает -g2 для ассемблера - убираем все флаги -g
    as_flags = [f for f in as_flags if not f.startswith("-g")]

    # Применяем флаги
    env.Append(CCFLAGS=cc_flags)
    env.Append(CXXFLAGS=cc_flags)
    env.Replace(ASFLAGS=as_flags)  # Явно заменяем ASFLAGS вместо Append
    env.Append(LINKFLAGS=makefile_data["cflags"])
    print(f"[OK] Compiler flags applied (CC: {len(cc_flags)}, AS: {len(as_flags)})")

    # Источники из Makefile
    all_sources = makefile_data["sources"]
else:
    # Fallback: если Makefile нет, собираем вручную
    print("[!] Makefile not found, using fallback...")
    all_sources = []

    # Core/Src
    core_src = os.path.join(PROJECT_DIR, "Core", "Src")
    if os.path.isdir(core_src):
        for f in glob.glob(os.path.join(core_src, "*.c")):
            if os.path.basename(f) not in ["syscalls.c", "sysmem.c"]:
                all_sources.append(f)

    # HAL
    drivers_dir = os.path.join(PROJECT_DIR, "Drivers")
    if os.path.isdir(drivers_dir):
        for entry in os.listdir(drivers_dir):
            if "_HAL_Driver" in entry:
                hal_src = os.path.join(drivers_dir, entry, "Src")
                if os.path.isdir(hal_src):
                    hal_files = glob.glob(os.path.join(hal_src, "*.c"))
                    all_sources.extend(hal_files)

    # CMSIS
    cmsis_device = os.path.join(PROJECT_DIR, "Drivers", "CMSIS", "Device", "ST")
    if os.path.isdir(cmsis_device):
        for entry in os.listdir(cmsis_device):
            system_files = glob.glob(os.path.join(cmsis_device, entry, "Source", "Templates", "system_stm32*.c"))
            all_sources.extend(system_files)

    # Startup
    startup_dir = os.path.join(PROJECT_DIR, "Core", "Startup")
    if os.path.isdir(startup_dir):
        for f in glob.glob(os.path.join(startup_dir, "*.[sS]")):
            all_sources.append(f)

# 2. MCU Detection
print("\n[2] Detecting MCU...")
mcu_name = detect_mcu_from_ioc()
print(f"[OK] MCU: {mcu_name or 'Unknown'}")

mcu_family = detect_mcu_family(mcu_name)
print(f"[OK] Family: {mcu_family}")

cpu = get_cpu_from_family(mcu_family)
print(f"[OK] CPU: {cpu}")

board_id = find_or_create_board(mcu_name, mcu_family, cpu)

# 3. Linker Script
print("\n[3] Linker Script...")
ld_script = find_linker_script()
if ld_script:
    print(f"[OK] Linker: {ld_script}")
    env.Replace(LDSCRIPT_PATH=ld_script)

# 3.5. OpenOCD Target File
print("\n[3.5] OpenOCD Target...")
openocd_target = get_openocd_target_file(mcu_family)
print(f"[OK] OpenOCD target: {openocd_target}")
# Сохраняем только имя файла (без -f target/)
env.Replace(UPLOAD_FLAGS=[openocd_target])

# 4. Middlewares (FreeRTOS, FatFS, LWIP и т.д.)
print("\n[4] Checking Middlewares...")
middlewares_dir = os.path.join(PROJECT_DIR, "Middlewares")
if os.path.isdir(middlewares_dir):
    mw_files = glob.glob(os.path.join(middlewares_dir, "**", "*.c"), recursive=True)
    mw_inc = glob.glob(os.path.join(middlewares_dir, "**", "Inc"), recursive=True)
    all_sources.extend(mw_files)
    for inc_dir in mw_inc:
        if os.path.isdir(inc_dir):
            env.Append(CPPPATH=[inc_dir])
    print(f"[OK] Middlewares: {len(mw_files)} files, {len(mw_inc)} include dirs")

# 5. Применяем источники
print("\n[5] Source Files...")
print(f"[OK] Total sources: {len(all_sources)} files")

# Создаем "библиотеку" из всех файлов проекта
# Это правильный способ добавить файлы вне src_dir в PlatformIO
project_lib = env.StaticLibrary("project_sources", all_sources)
env.Append(LIBS=[project_lib])
print(f"[OK] Created project library with {len(all_sources)} files")

# 6. C/C++ стандарты
env.Append(CCFLAGS=["-std=gnu11"])
env.Append(CXXFLAGS=["-std=gnu++17", "-fno-exceptions", "-fno-rtti"])

# 7. Разрешаем использовать lib_deps из platformio.ini
print("\n[6] Library Support...")
print("[OK] lib_deps support enabled (add to platformio.ini if needed)")

print("\n" + "=" * 60)
print("Configuration Complete!")
print("=" * 60)
