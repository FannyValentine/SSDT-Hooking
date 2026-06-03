/*============================================================================
    main.c - user mode консольная утилита для управления драйвером
    Соответствует ТЗ: раздел 5, пример сценария раздел 9
    Компиляция: gcc -o ssdtmon.exe main.c -lwinmm -static
============================================================================*/

#include <windows.h>
#include <stdio.h>
#include <time.h>
#include "common.h"

static HANDLE hDevice = INVALID_HANDLE_VALUE;

/*----------------------------------------------------------------------------
    OpenDevice - открытие драйвера
----------------------------------------------------------------------------*/
BOOL OpenDevice(VOID)
{
    hDevice = CreateFileW(
        DEVICE_NAME_SYMBOLIC,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    
    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("[-] Failed to open driver. Error: %d\n", GetLastError());
        printf("    Make sure SsdtMon driver is loaded.\n");
        printf("    Run this program as Administrator!\n");
        return FALSE;
    }
    
    return TRUE;
}

/*----------------------------------------------------------------------------
    CloseDevice - закрытие драйвера
----------------------------------------------------------------------------*/
VOID CloseDevice(VOID)
{
    if (hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(hDevice);
        hDevice = INVALID_HANDLE_VALUE;
    }
}

/*----------------------------------------------------------------------------
    InstallHook - установка хука (ТЗ раздел 5, IOCTL_SET_HOOK)
----------------------------------------------------------------------------*/
BOOL InstallHook(ULONG ServiceIndex, ULONG64 HookFunction)
{
    HOOK_REQUEST request;
    DWORD bytesReturned;
    BOOL result;
    
    request.ServiceIndex = ServiceIndex;
    request.HookFunction = HookFunction;
    
    result = DeviceIoControl(
        hDevice,
        IOCTL_SSDT_SET_HOOK,
        &request,
        sizeof(request),
        NULL,
        0,
        &bytesReturned,
        NULL);
    
    if (result) {
        printf("[+] Hook installed on index 0x%X\n", ServiceIndex);
    } else {
        printf("[-] Failed to install hook (error: %d)\n", GetLastError());
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    RemoveHook - снятие хука (ТЗ раздел 5, IOCTL_REMOVE_HOOK)
----------------------------------------------------------------------------*/
BOOL RemoveHook(ULONG ServiceIndex)
{
    DWORD bytesReturned;
    BOOL result;
    
    result = DeviceIoControl(
        hDevice,
        IOCTL_SSDT_REMOVE_HOOK,
        &ServiceIndex,
        sizeof(ServiceIndex),
        NULL,
        0,
        &bytesReturned,
        NULL);
    
    if (result) {
        printf("[+] Hook removed from index 0x%X\n", ServiceIndex);
    } else {
        printf("[-] Failed to remove hook (error: %d)\n", GetLastError());
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    StartMonitor - запуск мониторинга (ТЗ раздел 5, IOCTL_MONITOR_START)
----------------------------------------------------------------------------*/
BOOL StartMonitor(ULONG IntervalMs)
{
    DWORD bytesReturned;
    BOOL result;
    
    result = DeviceIoControl(
        hDevice,
        IOCTL_SSDT_MONITOR_START,
        &IntervalMs,
        sizeof(IntervalMs),
        NULL,
        0,
        &bytesReturned,
        NULL);
    
    if (result) {
        printf("[+] Monitor started (interval=%d ms)\n", IntervalMs);
    } else {
        printf("[-] Failed to start monitor (error: %d)\n", GetLastError());
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    StopMonitor - остановка мониторинга (ТЗ раздел 5, IOCTL_MONITOR_STOP)
----------------------------------------------------------------------------*/
BOOL StopMonitor(VOID)
{
    DWORD bytesReturned;
    BOOL result;
    
    result = DeviceIoControl(
        hDevice,
        IOCTL_SSDT_MONITOR_STOP,
        NULL,
        0,
        NULL,
        0,
        &bytesReturned,
        NULL);
    
    if (result) {
        printf("[+] Monitor stopped\n");
    } else {
        printf("[-] Failed to stop monitor (error: %d)\n", GetLastError());
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    GetLog - чтение лога событий (ТЗ раздел 5, IOCTL_GET_LOG)
----------------------------------------------------------------------------*/
BOOL GetLog(VOID)
{
    LOG_ENTRY logBuffer[256];
    DWORD bytesRead;
    BOOL result;
    ULONG i;
    SYSTEMTIME st;
    FILETIME ft;
    time_t rawtime;
    struct tm* timeinfo;
    
    result = DeviceIoControl(
        hDevice,
        IOCTL_SSDT_GET_LOG,
        NULL,
        0,
        logBuffer,
        sizeof(logBuffer),
        &bytesRead,
        NULL);
    
    if (!result) {
        printf("[-] Failed to read log (error: %d)\n", GetLastError());
        return FALSE;
    }
    
    printf("\n=== SSDT Monitor Log (%d events) ===\n\n", bytesRead / sizeof(LOG_ENTRY));
    
    for (i = 0; i < bytesRead / sizeof(LOG_ENTRY); i++) {
        // Конвертация FILETIME в читаемый формат
        ft.dwLowDateTime = logBuffer[i].Timestamp.LowPart;
        ft.dwHighDateTime = logBuffer[i].Timestamp.HighPart;
        FileTimeToSystemTime(&ft, &st);
        
        printf("[%02d:%02d:%02d.%03d] ", 
               st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        
        switch (logBuffer[i].ChangeType) {
            case CHANGE_TYPE_ADDRESS:
                printf("[ADDR CHANGE] Index 0x%04X: 0x%016llX -> 0x%016llX\n",
                       logBuffer[i].ServiceIndex,
                       logBuffer[i].ExpectedValue,
                       logBuffer[i].ActualValue);
                break;
                
            case CHANGE_TYPE_BYTECODE:
                printf("[BYTECODE CHANGE] Index 0x%04X: expected 0x%02llX, got 0x%02llX\n",
                       logBuffer[i].ServiceIndex,
                       logBuffer[i].ExpectedValue,
                       logBuffer[i].ActualValue);
                break;
                
            case CHANGE_TYPE_RESTORED:
                printf("[RESTORED] Index 0x%04X: restored to 0x%016llX\n",
                       logBuffer[i].ServiceIndex,
                       logBuffer[i].ActualValue);
                break;
                
            default:
                printf("[UNKNOWN] Index 0x%04X: type=%d\n",
                       logBuffer[i].ServiceIndex, logBuffer[i].ChangeType);
                break;
        }
    }
    
    printf("\n");
    return TRUE;
}

/*----------------------------------------------------------------------------
    ListHooks - получение списка активных хуков (ТЗ раздел 5, IOCTL_GET_HOOKS)
----------------------------------------------------------------------------*/
BOOL ListHooks(VOID)
{
    HOOK_INFO hookBuffer[256];
    DWORD bytesRead;
    BOOL result;
    ULONG i;
    ULONG hookCount;
    
    result = DeviceIoControl(
        hDevice,
        IOCTL_SSDT_GET_HOOKS,
        NULL,
        0,
        hookBuffer,
        sizeof(hookBuffer),
        &bytesRead,
        NULL);
    
    if (!result) {
        printf("[-] Failed to get hooks (error: %d)\n", GetLastError());
        return FALSE;
    }
    
    hookCount = bytesRead / sizeof(HOOK_INFO);
    
    if (hookCount == 0) {
        printf("No active hooks installed.\n");
        return TRUE;
    }
    
    printf("\n=== Active Hooks ===\n\n");
    printf("Idx     Original Address     Current Address      Status\n");
    printf("--------------------------------------------------------\n");
    
    for (i = 0; i < hookCount; i++) {
        printf("0x%04X  0x%016llX   0x%016llX   %s\n",
               hookBuffer[i].ServiceIndex,
               hookBuffer[i].OriginalAddress,
               hookBuffer[i].CurrentAddress,
               hookBuffer[i].IsHooked ? "HOOKED" : "---");
    }
    
    printf("\n");
    return TRUE;
}

/*----------------------------------------------------------------------------
    RestoreSSDT - восстановление таблицы (ТЗ раздел 5, IOCTL_RESTORE)
----------------------------------------------------------------------------*/
BOOL RestoreSSDT(VOID)
{
    DWORD bytesReturned;
    BOOL result;
    
    result = DeviceIoControl(
        hDevice,
        IOCTL_SSDT_RESTORE,
        NULL,
        0,
        NULL,
        0,
        &bytesReturned,
        NULL);
    
    if (result) {
        printf("[+] SSDT restored to original\n");
    } else {
        printf("[-] Failed to restore SSDT (error: %d)\n", GetLastError());
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    ClearLog - очистка лога (ТЗ раздел 5, IOCTL_CLEAR_LOG)
----------------------------------------------------------------------------*/
BOOL ClearLogCmd(VOID)
{
    DWORD bytesReturned;
    BOOL result;
    
    result = DeviceIoControl(
        hDevice,
        IOCTL_SSDT_CLEAR_LOG,
        NULL,
        0,
        NULL,
        0,
        &bytesReturned,
        NULL);
    
    if (result) {
        printf("[+] Log buffer cleared\n");
    } else {
        printf("[-] Failed to clear log (error: %d)\n", GetLastError());
    }
    
    return result;
}

/*----------------------------------------------------------------------------
    PrintHelp - справка по командам
----------------------------------------------------------------------------*/
void PrintHelp(void)
{
    printf("\n");
    printf("SSDT Integrity Monitor - User Mode Controller v2.0\n");
    printf("===================================================\n\n");
    printf("Usage: ssdtmon.exe <command> [args]\n\n");
    printf("Commands:\n");
    printf("  --set-hook <index> <address>   Install hook on syscall index\n");
    printf("  --remove-hook <index>          Remove hook from index\n");
    printf("  --list-hooks                   List all active hooks\n");
    printf("  --monitor-start [ms]           Start integrity monitor (default: 200ms)\n");
    printf("  --monitor-stop                 Stop integrity monitor\n");
    printf("  --get-log                      Display log buffer\n");
    printf("  --restore                      Restore original SSDT\n");
    printf("  --clear-log                    Clear log buffer\n");
    printf("  --help                         Show this help\n\n");
    printf("Example (согласно ТЗ раздел 9):\n");
    printf("  ssdtmon.exe --set-hook 0x7F 0xFFFFF80001234567\n");
    printf("  ssdtmon.exe --monitor-start 200\n");
    printf("  ssdtmon.exe --get-log\n");
    printf("  ssdtmon.exe --restore\n\n");
}

/*----------------------------------------------------------------------------
    main - точка входа
----------------------------------------------------------------------------*/
int main(int argc, char* argv[])
{
    if (argc < 2) {
        PrintHelp();
        return 1;
    }
    
    if (!OpenDevice()) {
        return 1;
    }
    
    if (strcmp(argv[1], "--set-hook") == 0 && argc >= 4) {
        ULONG index = (ULONG)strtoul(argv[2], NULL, 16);
        ULONG64 address = strtoull(argv[3], NULL, 16);
        InstallHook(index, address);
    }
    else if (strcmp(argv[1], "--remove-hook") == 0 && argc >= 3) {
        ULONG index = (ULONG)strtoul(argv[2], NULL, 16);
        RemoveHook(index);
    }
    else if (strcmp(argv[1], "--list-hooks") == 0) {
        ListHooks();
    }
    else if (strcmp(argv[1], "--monitor-start") == 0) {
        ULONG ms = (argc >= 3) ? atoi(argv[2]) : 200;
        StartMonitor(ms);
    }
    else if (strcmp(argv[1], "--monitor-stop") == 0) {
        StopMonitor();
    }
    else if (strcmp(argv[1], "--get-log") == 0) {
        GetLog();
    }
    else if (strcmp(argv[1], "--restore") == 0) {
        RestoreSSDT();
    }
    else if (strcmp(argv[1], "--clear-log") == 0) {
        ClearLogCmd();
    }
    else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        PrintHelp();
    }
    else {
        printf("[-] Unknown command: %s\n", argv[1]);
        PrintHelp();
    }
    
    CloseDevice();
    return 0;
}