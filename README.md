# SSDT Integrity Monitor & Hook Manager

## Описание

Драйвер для Windows 10 x64, выполняющий:
- Перехват системных вызовов через SSDT Hooking
- Мониторинг целостности таблицы системных вызовов
- Детектирование ручных изменений адресов и байт-кода
- Автоматическое восстановление (опционально)
- Логирование в кольцевой буфер (64KB)

## Требования

- Windows 10 x64 (1903 - 22H2)
- Visual Studio 2026 Community + WDK
- MinGW (для сборки user-mode утилиты)

## Сборка

1. Открыть `Developer Command Prompt for VS 2026`
2. Запустить `build_driver.bat`
3. Запустить `build_user.bat`

## Установка

1. Запустить `install.bat` от имени администратора
2. Проверить статус: `sc query SsdtMon`

## Использование

```bash
# Установка хука
ssdtmon.exe --set-hook 0x7F 0xFFFFF80001234567

# Включение мониторинга
ssdtmon.exe --monitor-start 200

# Просмотр лога
ssdtmon.exe --get-log

# Список активных хуков
ssdtmon.exe --list-hooks

# Восстановление SSDT
ssdtmon.exe --restore