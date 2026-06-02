/*============================================================================
    device.c - работа с драйвером из user mode (Windows 10/11)
    Соответствует ТЗ: разделы 5, 8, 9
============================================================================*/

#include <windows.h>
#include <stdio.h>
#include "common.h"

// Глобальные переменные
static HANDLE g_hDevice = INVALID_HANDLE_VALUE;
static CHAR g_LastError[256] = {0};

/*----------------------------------------------------------------------------
    GetLastErrorString - возвращает текстовое описание последней ошибки
----------------------------------------------------------------------------*/
PCSTR GetLastErrorString(VOID)
{
    DWORD errorCode = GetLastError();
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        0,
        g_LastError,
        sizeof(g_LastError),
        NULL);
    return g_LastError;
}

/*----------------------------------------------------------------------------
    OpenDevice - открытие драйвера
    Возвращает TRUE в случае успеха
----------------------------------------------------------------------------*/
BOOL OpenDevice(VOID)
{
    // Открываем драйвер через символическую ссылку
    g_hDevice = CreateFileW(
        DEVICE_NAME_SYMBOLIC,
        GENERIC_READ | GENERIC_WRITE,
        0,                          // Нет совместного доступа
        NULL,                       // Без атрибутов безопасности
        OPEN_EXISTING,              // Драйвер должен существовать
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    
    if (g_hDevice == INVALID_HANDLE_VALUE) {
        printf("[-] Failed to open device. Error: %s\n", GetLastErrorString());
        printf("    Make sure SsdtMon driver is loaded and running.\n");
        printf("    Run this program as Administrator!\n");
        return FALSE;
    }
    
    printf("[+] Device opened successfully\n");
    return TRUE;
}

/*----------------------------------------------------------------------------
    CloseDevice - закрытие драйвера
----------------------------------------------------------------------------*/
VOID CloseDevice(VOID)
{
    if (g_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hDevice);
        g_hDevice = INVALID_HANDLE_VALUE;
        printf("[+] Device closed\n");
    }
}

/*----------------------------------------------------------------------------
    SendIoctl - универсальная функция отправки IOCTL
----------------------------------------------------------------------------*/
BOOL SendIoctl(
    DWORD IoctlCode,
    PVOID InputBuffer,
    DWORD InputSize,
    PVOID OutputBuffer,
    DWORD OutputSize,
    PDWORD BytesReturned)
{
    DWORD bytesReturned = 0;
    BOOL result;
    
    if (g_hDevice == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    
    result = DeviceIoControl(
        g_hDevice,
        IoctlCode,
        InputBuffer,
        InputSize,
        OutputBuffer,
        OutputSize,
        &bytesReturned,
        NULL);
    
    if (BytesReturned) {
        *BytesReturned = bytesReturned;
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    InstallHook - установка хука на системный вызов
    Соответствует ТЗ: раздел 5, 9
----------------------------------------------------------------------------*/
BOOL InstallHook(ULONG ServiceIndex, ULONG64 HookFunction)
{
    HOOK_REQUEST request;
    DWORD bytesReturned;
    BOOL result;
    
    request.ServiceIndex = ServiceIndex;
    request.HookFunction = HookFunction;
    
    printf("[*] Installing hook on index 0x%X (0x%p)...\n", 
           ServiceIndex, (PVOID)HookFunction);
    
    result = SendIoctl(
        IOCTL_SSDT_SET_HOOK,
        &request,
        sizeof(request),
        NULL,
        0,
        &bytesReturned);
    
    if (result) {
        printf("[+] Hook installed successfully on index 0x%X\n", ServiceIndex);
    } else {
        printf("[-] Failed to install hook on index 0x%X. Error: %s\n", 
               ServiceIndex, GetLastErrorString());
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    RemoveHook - снятие хука с системного вызова
    Соответствует ТЗ: раздел 5
----------------------------------------------------------------------------*/
BOOL RemoveHook(ULONG ServiceIndex)
{
    DWORD bytesReturned;
    BOOL result;
    
    printf("[*] Removing hook from index 0x%X...\n", ServiceIndex);
    
    result = SendIoctl(
        IOCTL_SSDT_REMOVE_HOOK,
        &ServiceIndex,
        sizeof(ServiceIndex),
        NULL,
        0,
        &bytesReturned);
    
    if (result) {
        printf("[+] Hook removed successfully from index 0x%X\n", ServiceIndex);
    } else {
        printf("[-] Failed to remove hook from index 0x%X. Error: %s\n", 
               ServiceIndex, GetLastErrorString());
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    GetHooks - получение списка активных хуков
    Соответствует ТЗ: раздел 5
----------------------------------------------------------------------------*/
BOOL GetHooks(PHOOK_INFO Hooks, ULONG MaxHooks, PULONG ReturnedCount)
{
    DWORD bytesReturned;
    BOOL result;
    
    result = SendIoctl(
        IOCTL_SSDT_GET_HOOKS,
        NULL,
        0,
        Hooks,
        MaxHooks * sizeof(HOOK_INFO),
        &bytesReturned);
    
    if (result && ReturnedCount) {
        *ReturnedCount = bytesReturned / sizeof(HOOK_INFO);
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    PrintHooks - вывод списка активных хуков в консоль
----------------------------------------------------------------------------*/
BOOL PrintHooks(VOID)
{
    HOOK_INFO hooks[256];
    ULONG count = 0;
    ULONG i;
    
    if (!GetHooks(hooks, 256, &count)) {
        printf("[-] Failed to get hooks list. Error: %s\n", GetLastErrorString());
        return FALSE;
    }
    
    if (count == 0) {
        printf("[*] No active hooks found\n");
        return TRUE;
    }
    
    printf("\n=== Active SSDT Hooks (%d) ===\n", count);
    printf("%-10s %-20s %-20s %-10s\n", 
           "Index", "Original Address", "Current Address", "Status");
    printf("%-10s %-20s %-20s %-10s\n", 
           "-----", "----------------", "--------------", "------");
    
    for (i = 0; i < count; i++) {
        printf("0x%-8X 0x%-16p 0x%-16p %s\n",
               hooks[i].ServiceIndex,
               (PVOID)hooks[i].OriginalAddress,
               (PVOID)hooks[i].CurrentAddress,
               hooks[i].IsHooked ? "HOOKED" : "NORMAL");
    }
    
    printf("\n");
    return TRUE;
}

/*----------------------------------------------------------------------------
    StartMonitor - запуск мониторинга целостности
    Соответствует ТЗ: разделы 2.1, 5, 9
----------------------------------------------------------------------------*/
BOOL StartMonitor(ULONG IntervalMs)
{
    DWORD bytesReturned;
    BOOL result;
    
    printf("[*] Starting integrity monitor (interval=%d ms)...\n", IntervalMs);
    
    result = SendIoctl(
        IOCTL_SSDT_MONITOR_START,
        &IntervalMs,
        sizeof(IntervalMs),
        NULL,
        0,
        &bytesReturned);
    
    if (result) {
        printf("[+] Monitor started successfully\n");
    } else {
        printf("[-] Failed to start monitor. Error: %s\n", GetLastErrorString());
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    StopMonitor - остановка мониторинга
    Соответствует ТЗ: раздел 5
----------------------------------------------------------------------------*/
BOOL StopMonitor(VOID)
{
    DWORD bytesReturned;
    BOOL result;
    
    printf("[*] Stopping integrity monitor...\n");
    
    result = SendIoctl(
        IOCTL_SSDT_MONITOR_STOP,
        NULL,
        0,
        NULL,
        0,
        &bytesReturned);
    
    if (result) {
        printf("[+] Monitor stopped successfully\n");
    } else {
        printf("[-] Failed to stop monitor. Error: %s\n", GetLastErrorString());
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    GetLog - чтение лога событий
    Соответствует ТЗ: разделы 2.2, 5
----------------------------------------------------------------------------*/
BOOL GetLog(PLOG_ENTRY Buffer, ULONG MaxEntries, PULONG ReturnedEntries)
{
    DWORD bytesReturned;
    BOOL result;
    
    result = SendIoctl(
        IOCTL_SSDT_GET_LOG,
        NULL,
        0,
        Buffer,
        MaxEntries * sizeof(LOG_ENTRY),
        &bytesReturned);
    
    if (result && ReturnedEntries) {
        *ReturnedEntries = bytesReturned / sizeof(LOG_ENTRY);
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    PrintLog - вывод лога событий в консоль
    Соответствует ТЗ: раздел 9 (пример сценария)
----------------------------------------------------------------------------*/
BOOL PrintLog(VOID)
{
    LOG_ENTRY logBuffer[256];
    ULONG count = 0;
    ULONG i;
    SYSTEMTIME st;
    FILETIME ft;
    
    if (!GetLog(logBuffer, 256, &count)) {
        printf("[-] Failed to read log. Error: %s\n", GetLastErrorString());
        return FALSE;
    }
    
    if (count == 0) {
        printf("[*] Log is empty\n");
        return TRUE;
    }
    
    printf("\n=== SSDT Monitor Log (%d events) ===\n", count);
    
    for (i = 0; i < count; i++) {
        // Конвертация времени
        ft.dwLowDateTime = logBuffer[i].Timestamp.LowPart;
        ft.dwHighDateTime = logBuffer[i].Timestamp.HighPart;
        FileTimeToSystemTime(&ft, &st);
        
        printf("[%02d:%02d:%02d.%03d] ",
               st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        
        switch (logBuffer[i].ChangeType) {
            case CHANGE_TYPE_ADDRESS:
                printf("[ADDRESS CHANGE] Index 0x%X: 0x%p -> 0x%p\n",
                       logBuffer[i].ServiceIndex,
                       (PVOID)logBuffer[i].ExpectedValue,
                       (PVOID)logBuffer[i].ActualValue);
                break;
                
            case CHANGE_TYPE_BYTECODE:
                printf("[BYTECODE CHANGE] Index 0x%X: mismatch at offset\n",
                       logBuffer[i].ServiceIndex);
                break;
                
            case CHANGE_TYPE_RESTORED:
                printf("[AUTO-RESTORED] Index 0x%X: restored to 0x%p\n",
                       logBuffer[i].ServiceIndex,
                       (PVOID)logBuffer[i].ActualValue);
                break;
                
            default:
                printf("[UNKNOWN] Index 0x%X\n", logBuffer[i].ServiceIndex);
                break;
        }
    }
    
    printf("\n");
    return TRUE;
}

/*----------------------------------------------------------------------------
    RestoreSSDT - восстановление оригинального SSDT
    Соответствует ТЗ: разделы 2.4 (use-case 3), 5
----------------------------------------------------------------------------*/
BOOL RestoreSSDT(VOID)
{
    DWORD bytesReturned;
    BOOL result;
    
    printf("[*] Restoring original SSDT...\n");
    
    result = SendIoctl(
        IOCTL_SSDT_RESTORE,
        NULL,
        0,
        NULL,
        0,
        &bytesReturned);
    
    if (result) {
        printf("[+] SSDT restored successfully\n");
    } else {
        printf("[-] Failed to restore SSDT. Error: %s\n", GetLastErrorString());
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    ClearLog - очистка лога событий
    Соответствует ТЗ: раздел 5
----------------------------------------------------------------------------*/
BOOL ClearLog(VOID)
{
    DWORD bytesReturned;
    BOOL result;
    
    printf("[*] Clearing log buffer...\n");
    
    result = SendIoctl(
        IOCTL_SSDT_CLEAR_LOG,
        NULL,
        0,
        NULL,
        0,
        &bytesReturned);
    
    if (result) {
        printf("[+] Log cleared successfully\n");
    } else {
        printf("[-] Failed to clear log. Error: %s\n", GetLastErrorString());
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    IsDriverLoaded - проверка, загружен ли драйвер
----------------------------------------------------------------------------*/
BOOL IsDriverLoaded(VOID)
{
    HANDLE hTest;
    
    hTest = CreateFileW(
        DEVICE_NAME_SYMBOLIC,
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    
    if (hTest != INVALID_HANDLE_VALUE) {
        CloseHandle(hTest);
        return TRUE;
    }
    
    return FALSE;
}