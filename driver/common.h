/*============================================================================
    common.h - общие определения для драйвера и user mode утилиты
    Версия: 2.0
    Соответствует ТЗ: разделы 2, 5
============================================================================*/

#ifndef _COMMON_H_
#define _COMMON_H_

#include <windows.h>

#define DEVICE_NAME_SYMBOLIC L"\\\\.\\SsdtMonWin10"

// IOCTL коды (согласно ТЗ раздел 5)
#define IOCTL_SSDT_GET_HOOKS      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SSDT_SET_HOOK       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SSDT_REMOVE_HOOK    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SSDT_MONITOR_START  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SSDT_MONITOR_STOP   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SSDT_GET_LOG        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SSDT_RESTORE        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SSDT_CLEAR_LOG      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Типы изменений (согласно ТЗ раздел 2.1)
#define CHANGE_TYPE_ADDRESS       0x01   // Адрес функции изменён
#define CHANGE_TYPE_BYTECODE      0x02   // Байт-код в прологе изменён
#define CHANGE_TYPE_RESTORED      0x03   // Восстановлено

#pragma pack(push, 1)

// Структура записи лога (согласно ТЗ раздел 3.1)
typedef struct _LOG_ENTRY {
    LARGE_INTEGER   Timestamp;       // Время события
    ULONG           ServiceIndex;    // Индекс системного вызова
    UCHAR           ChangeType;      // Тип изменения
    ULONG64         ExpectedValue;   // Ожидаемое значение
    ULONG64         ActualValue;     // Фактическое значение
    BOOLEAN         WasRestored;     // Было ли восстановлено
} LOG_ENTRY;

// Структура информации о хуке (согласно ТЗ раздел 5)
typedef struct _HOOK_INFO {
    ULONG   ServiceIndex;            // Индекс
    ULONG64 OriginalAddress;         // Оригинальный адрес
    ULONG64 CurrentAddress;          // Текущий адрес
    BOOLEAN IsHooked;                // Хук активен?
} HOOK_INFO;

// Структура запроса на установку хука (согласно ТЗ раздел 5)
typedef struct _HOOK_REQUEST {
    ULONG   ServiceIndex;            // Какой системный вызов перехватить
    ULONG64 HookFunction;            // Адрес функции-обработчика
} HOOK_REQUEST;

#pragma pack(pop)

#endif // _COMMON_H_