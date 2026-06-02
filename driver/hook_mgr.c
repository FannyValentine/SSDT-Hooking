/*============================================================================
    hook_mgr.c - управление установкой и снятием хуков SSDT
    Соответствует ТЗ: разделы 2.3, 2.4, 4.2
============================================================================*/

#include <ntddk.h>
#include "common.h"

// Внешние переменные
extern ULONG64* KiServiceTable;
extern ULONG64* GoldenAddressTable;
extern ULONG64* HookTable;
extern ULONG KiServiceLimit;
extern KSPIN_LOCK TableLock;

// Локальные переменные
static ULONG g_HookCount = 0;
static LIST_ENTRY g_HookListHead;  // Список всех активных хуков

/*============================================================================
    Структура для хранения информации об одном хуке
    (для расширяемости и будущих use-case)
============================================================================*/
typedef struct _HOOK_ENTRY {
    LIST_ENTRY ListEntry;
    ULONG ServiceIndex;
    ULONG64 OriginalAddress;
    ULONG64 HookAddress;
    ULONG64 HookHandler;           // Указатель на функцию-обработчик
    ULONG CallCount;               // Счётчик вызовов (use-case 2)
    ULONG PolicyFlags;             // Флаги политики (use-case 1, белый список)
    BOOLEAN IsActive;
} HOOK_ENTRY, *PHOOK_ENTRY;

// Флаги политик для хуков
#define HOOK_POLICY_LOG_ONLY       0x0001  // Только логировать
#define HOOK_POLICY_BLOCK          0x0002  // Блокировать вызов
#define HOOK_POLICY_MODIFY         0x0004  // Модифицировать параметры
#define HOOK_POLICY_WHITELIST      0x0008  // Белый список (разрешённые вызовы)

/*----------------------------------------------------------------------------
    InitHookManager - инициализация менеджера хуков
----------------------------------------------------------------------------*/
NTSTATUS InitHookManager(VOID)
{
    InitializeListHead(&g_HookListHead);
    g_HookCount = 0;
    
    DbgPrint("SsdtMon: Hook Manager initialized\n");
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    FindHookEntry - поиск записи о хуке по индексу
----------------------------------------------------------------------------*/
PHOOK_ENTRY FindHookEntry(ULONG ServiceIndex)
{
    PLIST_ENTRY current;
    PHOOK_ENTRY entry;
    
    for (current = g_HookListHead.Flink; 
         current != &g_HookListHead; 
         current = current->Flink) {
        
        entry = CONTAINING_RECORD(current, HOOK_ENTRY, ListEntry);
        if (entry->ServiceIndex == ServiceIndex && entry->IsActive) {
            return entry;
        }
    }
    
    return NULL;
}

/*----------------------------------------------------------------------------
    AddHookEntry - добавление записи о хуке в список
----------------------------------------------------------------------------*/
NTSTATUS AddHookEntry(ULONG ServiceIndex, ULONG64 OriginalAddress, ULONG64 HookAddress)
{
    PHOOK_ENTRY newEntry;
    KIRQL oldIrql;
    
    newEntry = (PHOOK_ENTRY)ExAllocatePoolWithTag(
        NonPagedPoolNx, sizeof(HOOK_ENTRY), 'tdSS');
    
    if (!newEntry) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    newEntry->ServiceIndex = ServiceIndex;
    newEntry->OriginalAddress = OriginalAddress;
    newEntry->HookAddress = HookAddress;
    newEntry->HookHandler = HookAddress;
    newEntry->CallCount = 0;
    newEntry->PolicyFlags = HOOK_POLICY_LOG_ONLY;  // По умолчанию только логируем
    newEntry->IsActive = TRUE;
    
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    InsertHeadList(&g_HookListHead, &newEntry->ListEntry);
    g_HookCount++;
    KeLowerIrql(oldIrql);
    
    DbgPrint("SsdtMon: Hook entry added for index 0x%X (total: %d)\n", 
             ServiceIndex, g_HookCount);
    
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    RemoveHookEntry - удаление записи о хуке из списка
----------------------------------------------------------------------------*/
NTSTATUS RemoveHookEntry(ULONG ServiceIndex)
{
    PHOOK_ENTRY entry;
    KIRQL oldIrql;
    
    entry = FindHookEntry(ServiceIndex);
    if (!entry) {
        return STATUS_NOT_FOUND;
    }
    
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    RemoveEntryList(&entry->ListEntry);
    g_HookCount--;
    KeLowerIrql(oldIrql);
    
    ExFreePoolWithTag(entry, 'tdSS');
    
    DbgPrint("SsdtMon: Hook entry removed for index 0x%X (total: %d)\n", 
             ServiceIndex, g_HookCount);
    
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    IncrementHookCallCount - увеличение счётчика вызовов хука (для статистики)
    Соответствует ТЗ: раздел 2.4 (use-case 2 - сбор статистики)
----------------------------------------------------------------------------*/
ULONG IncrementHookCallCount(ULONG ServiceIndex)
{
    PHOOK_ENTRY entry;
    ULONG count;
    KIRQL oldIrql;
    
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    
    entry = FindHookEntry(ServiceIndex);
    if (entry) {
        entry->CallCount++;
        count = entry->CallCount;
    } else {
        count = 0;
    }
    
    KeLowerIrql(oldIrql);
    return count;
}

/*----------------------------------------------------------------------------
    SetHookPolicy - установка политики для хука
    Соответствует ТЗ: раздел 2.4 (use-case 1 - белый список, use-case 4 - эмуляция)
----------------------------------------------------------------------------*/
NTSTATUS SetHookPolicy(ULONG ServiceIndex, ULONG PolicyFlags)
{
    PHOOK_ENTRY entry;
    KIRQL oldIrql;
    
    entry = FindHookEntry(ServiceIndex);
    if (!entry) {
        return STATUS_NOT_FOUND;
    }
    
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    entry->PolicyFlags = PolicyFlags;
    KeLowerIrql(oldIrql);
    
    DbgPrint("SsdtMon: Policy for index 0x%X set to 0x%X\n", ServiceIndex, PolicyFlags);
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    GetHookStatistics - получение статистики по хуку (для use-case 2)
----------------------------------------------------------------------------*/
NTSTATUS GetHookStatistics(ULONG ServiceIndex, PULONG OutCallCount, PULONG OutPolicyFlags)
{
    PHOOK_ENTRY entry;
    KIRQL oldIrql;
    
    entry = FindHookEntry(ServiceIndex);
    if (!entry) {
        return STATUS_NOT_FOUND;
    }
    
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    if (OutCallCount) *OutCallCount = entry->CallCount;
    if (OutPolicyFlags) *OutPolicyFlags = entry->PolicyFlags;
    KeLowerIrql(oldIrql);
    
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    IsWhitelisted - проверка, разрешён ли системный вызов (use-case 1)
    Соответствует ТЗ: раздел 2.4 (use-case 1 - белый список)
----------------------------------------------------------------------------*/
BOOLEAN IsWhitelisted(ULONG ServiceIndex)
{
    PHOOK_ENTRY entry;
    BOOLEAN result = FALSE;
    KIRQL oldIrql;
    
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    
    entry = FindHookEntry(ServiceIndex);
    if (entry && (entry->PolicyFlags & HOOK_POLICY_WHITELIST)) {
        result = TRUE;
    }
    
    KeLowerIrql(oldIrql);
    return result;
}

/*----------------------------------------------------------------------------
    InstallHook - установка хука на системный вызов
    Соответствует ТЗ: разделы 2.3, 4.2
----------------------------------------------------------------------------*/
NTSTATUS InstallHook(ULONG ServiceIndex, ULONG64 HookFunction)
{
    ULONG64 originalAddress;
    KIRQL oldIrql;
    NTSTATUS status;
    
    // Проверка валидности индекса
    if (ServiceIndex >= KiServiceLimit) {
        DbgPrint("SsdtMon: Invalid service index 0x%X (max: 0x%X)\n", 
                 ServiceIndex, KiServiceLimit - 1);
        return STATUS_INVALID_PARAMETER;
    }
    
    // Проверка адреса хук-функции
    if (!HookFunction || !MmIsAddressValid((PVOID)HookFunction)) {
        DbgPrint("SsdtMon: Invalid hook function address 0x%p\n", (PVOID)HookFunction);
        return STATUS_INVALID_PARAMETER;
    }
    
    // Получаем оригинальный адрес
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    originalAddress = KiServiceTable[ServiceIndex];
    KeLowerIrql(oldIrql);
    
    // Если хук уже существует, снимаем его
    if (HookTable[ServiceIndex] != 0) {
        DbgPrint("SsdtMon: Removing existing hook on index 0x%X\n", ServiceIndex);
        RemoveHook(ServiceIndex);
    }
    
    // Добавляем запись в список хуков
    status = AddHookEntry(ServiceIndex, originalAddress, HookFunction);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    // Заменяем адрес в SSDT
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    KiServiceTable[ServiceIndex] = HookFunction;
    HookTable[ServiceIndex] = HookFunction;
    
    // Обновляем эталонную копию (для мониторинга)
    if (GoldenAddressTable) {
        GoldenAddressTable[ServiceIndex] = HookFunction;
    }
    KeLowerIrql(oldIrql);
    
    DbgPrint("SsdtMon: Hook installed on index 0x%X (orig=0x%p, hook=0x%p)\n",
             ServiceIndex, (PVOID)originalAddress, (PVOID)HookFunction);
    
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    RemoveHook - снятие хука с системного вызова
    Соответствует ТЗ: разделы 2.3, 4.2
----------------------------------------------------------------------------*/
NTSTATUS RemoveHook(ULONG ServiceIndex)
{
    ULONG64 originalAddress;
    KIRQL oldIrql;
    NTSTATUS status;
    
    // Проверка валидности индекса
    if (ServiceIndex >= KiServiceLimit) {
        return STATUS_INVALID_PARAMETER;
    }
    
    // Проверяем, существует ли хук
    if (HookTable[ServiceIndex] == 0) {
        DbgPrint("SsdtMon: No hook found on index 0x%X\n", ServiceIndex);
        return STATUS_NOT_FOUND;
    }
    
    // Получаем оригинальный адрес из эталона
    if (GoldenAddressTable) {
        originalAddress = GoldenAddressTable[ServiceIndex];
    } else {
        KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
        originalAddress = KiServiceTable[ServiceIndex];
        KeLowerIrql(oldIrql);
    }
    
    // Удаляем запись из списка хуков
    status = RemoveHookEntry(ServiceIndex);
    if (!NT_SUCCESS(status)) {
        DbgPrint("SsdtMon: Failed to remove hook entry for index 0x%X\n", ServiceIndex);
    }
    
    // Восстанавливаем оригинальный адрес в SSDT
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    KiServiceTable[ServiceIndex] = originalAddress;
    HookTable[ServiceIndex] = 0;
    
    // Обновляем эталонную копию
    if (GoldenAddressTable) {
        GoldenAddressTable[ServiceIndex] = originalAddress;
    }
    KeLowerIrql(oldIrql);
    
    DbgPrint("SsdtMon: Hook removed from index 0x%X (restored to 0x%p)\n",
             ServiceIndex, (PVOID)originalAddress);
    
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    RemoveAllHooks - снятие всех установленных хуков
    Соответствует ТЗ: раздел 8 (критерии приёмки - корректная выгрузка)
----------------------------------------------------------------------------*/
NTSTATUS RemoveAllHooks(VOID)
{
    ULONG i;
    KIRQL oldIrql;
    ULONG removedCount = 0;
    
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    
    for (i = 0; i < KiServiceLimit; i++) {
        if (HookTable[i] != 0) {
            if (GoldenAddressTable) {
                KiServiceTable[i] = GoldenAddressTable[i];
            }
            HookTable[i] = 0;
            removedCount++;
        }
    }
    
    KeLowerIrql(oldIrql);
    
    DbgPrint("SsdtMon: Removed all %d hooks\n", removedCount);
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    GetHookCount - получение количества активных хуков
----------------------------------------------------------------------------*/
ULONG GetHookCount(VOID)
{
    return g_HookCount;
}

/*----------------------------------------------------------------------------
    HookHandlerTemplate - шаблон функции-обработчика хука
    (пример для будущих use-case)
----------------------------------------------------------------------------*/
/*
NTSTATUS MyHookHandler(
    ULONG ServiceIndex,
    PVOID SystemCallArguments,
    PVOID OriginalFunction)
{
    // Логирование вызова (use-case 2)
    IncrementHookCallCount(ServiceIndex);
    
    // Проверка белого списка (use-case 1)
    if (!IsWhitelisted(ServiceIndex)) {
        DbgPrint("SsdtMon: Blocked syscall index 0x%X\n", ServiceIndex);
        return STATUS_ACCESS_DENIED;
    }
    
    // Модификация параметров (по необходимости)
    // ...
    
    // Вызов оригинальной функции
    // return ((NTSTATUS (*)(PVOID))OriginalFunction)(SystemCallArguments);
    
    return STATUS_SUCCESS;
}
*/