/*============================================================================
    monitor.c - периодическая проверка целостности SSDT
    Соответствует ТЗ: разделы 2.1, 2.4 use-case 2
============================================================================*/

#include <ntddk.h>
#include "common.h"

// Внешние переменные
extern ULONG64* KiServiceTable;
extern ULONG64* GoldenAddressTable;
extern ULONG KiServiceLimit;
extern BOOLEAN MonitoringActive;
extern BOOLEAN AutoRestore;

// Локальные переменные
static KTIMER MonitorTimer;
static KDPC MonitorDpc;
static ULONG MonitorInterval = 200;  // мс, согласно ТЗ раздел 9

// Внешние функции
NTSTATUS LogEvent(ULONG ServiceIndex, UCHAR ChangeType, ULONG64 Expected, ULONG64 Actual);
NTSTATUS SetSSDTEntry(ULONG Index, ULONG64 Address);

/*----------------------------------------------------------------------------
    CheckSSDTIntegrity - проверка адресов (ТЗ раздел 2.1)
----------------------------------------------------------------------------*/
NTSTATUS CheckSSDTIntegrity(VOID)
{
    ULONG i;
    BOOLEAN changed = FALSE;
    
    for (i = 0; i < KiServiceLimit; i++) {
        ULONG64 current = KiServiceTable[i];
        ULONG64 expected = GoldenAddressTable[i];
        
        if (current != expected) {
            DbgPrint("SsdtMon: [WARNING] SSDT[0x%X] changed: expected 0x%p, current 0x%p\n",
                     i, (PVOID)expected, (PVOID)current);
            
            // Запись в лог (ТЗ раздел 2.2)
            LogEvent(i, CHANGE_TYPE_ADDRESS, expected, current);
            changed = TRUE;
            
            // Автоматическое восстановление (ТЗ раздел 2.4 use-case 3)
            if (AutoRestore) {
                KiServiceTable[i] = expected;
                LogEvent(i, CHANGE_TYPE_RESTORED, current, expected);
                DbgPrint("SsdtMon: Auto-restored index 0x%X\n", i);
            }
        }
    }
    
    return changed ? STATUS_DETECTED : STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    MonitorWorker - DPC-функция периодической проверки (ТЗ раздел 2.1)
----------------------------------------------------------------------------*/
VOID MonitorWorker(PKDPC Dpc, PVOID Context, PVOID Arg1, PVOID Arg2)
{
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(Arg1);
    UNREFERENCED_PARAMETER(Arg2);
    
    if (MonitoringActive) {
        CheckSSDTIntegrity();
        
        LARGE_INTEGER dueTime;
        dueTime.QuadPart = -10000 * (LONGLONG)MonitorInterval;
        KeSetTimerEx(&MonitorTimer, dueTime, MonitorInterval, &MonitorDpc);
    }
}

/*----------------------------------------------------------------------------
    StartMonitor - запуск мониторинга (ТЗ раздел 5)
----------------------------------------------------------------------------*/
NTSTATUS StartMonitor(ULONG IntervalMs)
{
    if (MonitoringActive) {
        return STATUS_ALREADY_COMMITTED;
    }
    
    MonitoringActive = TRUE;
    MonitorInterval = (IntervalMs > 0) ? IntervalMs : 200;
    
    KeInitializeTimer(&MonitorTimer);
    KeInitializeDpc(&MonitorDpc, MonitorWorker, NULL);
    
    LARGE_INTEGER dueTime;
    dueTime.QuadPart = -10000 * (LONGLONG)MonitorInterval;
    KeSetTimerEx(&MonitorTimer, dueTime, MonitorInterval, &MonitorDpc);
    
    DbgPrint("SsdtMon: Monitor started (interval=%d ms)\n", MonitorInterval);
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    StopMonitor - остановка мониторинга (ТЗ раздел 5)
----------------------------------------------------------------------------*/
NTSTATUS StopMonitor(VOID)
{
    if (!MonitoringActive) {
        return STATUS_NOT_FOUND;
    }
    
    MonitoringActive = FALSE;
    KeCancelTimer(&MonitorTimer);
    
    DbgPrint("SsdtMon: Monitor stopped\n");
    return STATUS_SUCCESS;
}