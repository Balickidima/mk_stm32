"""
Post-скрипт для исправления ASFLAGS
Убирает флаги отладки (-g*) из ассемблера для нового GCC 14.x
"""
Import("env")

# Получаем текущие ASFLAGS
current_asflags = env.get("ASFLAGS", [])

# Конвертируем в список если строка
if isinstance(current_asflags, str):
    current_asflags = current_asflags.split()

# Фильтруем - убираем все флаги начинающиеся с -g
filtered = [f for f in current_asflags if not f.startswith("-g")]

# Явно заменяем ASFLAGS
env.Replace(ASFLAGS=filtered)

print(f"[OK] ASFLAGS post-fixed: removed -g* flags")
print(f"     Before: {current_asflags}")
print(f"     After:  {filtered}")
