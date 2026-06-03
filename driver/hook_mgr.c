/*============================================================================
    hook_mgr.c - установка и снятие хуков на системные вызовы
    Соответствует ТЗ: разделы 2.3, 5
============================================================================*/

#include <ntddk.h>
#include "common.h"

// Внешние переменные
extern ULONG64* KiServiceTable;
extern ULONG64* GoldenAddressTable;
extern ULONG64* HookTable;
extern ULONG KiServiceLimit;

// Внешние функции
extern NTSTATUS SetSSDTEntry(ULONG Index, ULONG64 Address);
extern NTSTATUS GetSSDTEntry(ULONG Index, ULONG64* OutAddress);
extern NTSTATUS LogEvent(ULONG ServiceIndex, UCHAR ChangeType, ULONG64 Expected, ULONG64 Actual);

/*----------------------------------------------------------------------------
    InstallHook - установка хука (ТЗ раздел 2.3)
----------------------------------------------------------------------------*/
NTSTATUS InstallHook(ULONG ServiceIndex, ULONG64 HookFunction)
{
    ULONG64 originalAddress;
    NTSTATUS status;
    
    if (ServiceIndex >= KiServiceLimit) {
        return STATUS_INVALID_PARAMETER;
    }
    
    if (!HookFunction || !MmIsAddressValid((PVOID)HookFunction)) {
        return STATUS_INVALID_PARAMETER;
    }
    
    status = GetSSDTEntry(ServiceIndex, &originalAddress);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    // Если уже есть хук, снимаем его
    if (HookTable[ServiceIndex] != 0) {
        RemoveHook(ServiceIndex);
    }
    
    // Сохраняем информацию о хуке
    HookTable[ServiceIndex] = HookFunction;
    
    // Заменяем адрес в SSDT
    status = SetSSDTEntry(ServiceIndex, HookFunction);
    if (!NT_SUCCESS(status)) {
        HookTable[ServiceIndex] = 0;
        return status;
    }
    
    // Обновляем эталонную копию (для мониторинга)
    if (GoldenAddressTable) {
        GoldenAddressTable[ServiceIndex] = HookFunction;
    }
    
    DbgPrint("SsdtMon: Hook installed on index 0x%X (orig=0x%p, hook=0x%p)\n",
             ServiceIndex, (PVOID)originalAddress, (PVOID)HookFunction);
    
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    RemoveHook - снятие хука (восстановление оригинала)
----------------------------------------------------------------------------*/
NTSTATUS RemoveHook(ULONG ServiceIndex)
{
    ULONG64 originalAddress;
    NTSTATUS status;
    
    if (ServiceIndex >= KiServiceLimit) {
        return STATUS_INVALID_PARAMETER;
    }
    
    if (HookTable[ServiceIndex] == 0) {
        return STATUS_NOT_FOUND;
    }
    
    // Восстанавливаем оригинальный адрес из эталона
    if (GoldenAddressTable) {
        originalAddress = GoldenAddressTable[ServiceIndex];
        status = SetSSDTEntry(ServiceIndex, originalAddress);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        
        HookTable[ServiceIndex] = 0;
        
        DbgPrint("SsdtMon: Hook removed from index 0x%X (restored=0x%p)\n",
                 ServiceIndex, (PVOID)originalAddress);
        
        return STATUS_SUCCESS;
    }
    
    return STATUS_UNSUCCESSFUL;
}