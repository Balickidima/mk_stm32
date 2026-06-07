#!/usr/bin/env python3
"""
Patch script for STM32 HAL after CubeMX regeneration
Adds 'const' to HAL_RCC_OscConfig and HAL_RCC_ClockConfig declarations
to match the definitions in the .c files.

Supports: STM32F0, F1, F3, F4, F7, G0, G4, H7, L0, L1, L4, U5, WB, WL

Usage: python fix_hal_const.py
"""

import os
import re
import glob

def find_hal_rcc_header():
    """Ищет stm32*_hal_rcc.h в папке Drivers"""
    drivers_dir = os.path.join(os.path.dirname(__file__), "Drivers")
    if not os.path.exists(drivers_dir):
        return None
    
    # Ищем любую папку *_HAL_Driver
    for entry in os.listdir(drivers_dir):
        if "_HAL_Driver" in entry:
            hal_rcc_path = os.path.join(
                drivers_dir, entry, "Inc", "stm32*_hal_rcc.h"
            )
            matches = glob.glob(hal_rcc_path)
            if matches:
                return matches[0]
    return None


def fix_hal_const():
    hal_rcc_path = find_hal_rcc_header()
    
    if not hal_rcc_path:
        print("ERROR: stm32*_hal_rcc.h not found!")
        print("Make sure you generated code in CubeMX first.")
        return False

    print(f"Patching: {os.path.basename(hal_rcc_path)}")

    with open(hal_rcc_path, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # Fix HAL_RCC_OscConfig declaration - ADD const to match .c file
    content = re.sub(
        r'HAL_StatusTypeDef HAL_RCC_OscConfig\(RCC_OscInitTypeDef \*',
        r'HAL_StatusTypeDef HAL_RCC_OscConfig(const RCC_OscInitTypeDef *',
        content
    )

    # Fix HAL_RCC_ClockConfig declaration - ADD const to match .c file
    content = re.sub(
        r'HAL_StatusTypeDef HAL_RCC_ClockConfig\(RCC_ClkInitTypeDef \*',
        r'HAL_StatusTypeDef HAL_RCC_ClockConfig(const RCC_ClkInitTypeDef *',
        content
    )

    if content != original:
        with open(hal_rcc_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print("✓ HAL RCC header patched successfully!")
        return True
    else:
        print("✓ HAL RCC header already patched (no changes needed)")
        return True

if __name__ == "__main__":
    fix_hal_const()
