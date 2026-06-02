/*============================================================================
    ssdt_access.c - поиск SSDT и работа с таблицей
    Соответствует ТЗ: разделы 4.1, 4.2
============================================================================*/

#include <ntddk.h>
#include "common.h"

// Внешние глобальные переменные
extern PSYSTEM_SERVICE_DESCRIPTOR_TABLE KeServiceDescriptorTable;
extern ULONG64* KiServiceTable;
extern ULONG KiServiceLimit;
extern ULONG64* GoldenAddressTable;
extern ULONG64* HookTable;

/*----------------------------------------------------------------------------
    FindKeServiceDescriptorTableWin10 - поиск SSDT через сигнатуры (ТЗ 4.1)
    Для Windows 10 x64 KeServiceDescriptorTable не экспортируется,
    поэтому используем поиск по сигнатуре в районе KiSystemCall64
----------------------------------------------------------------------------*/
NTSTATUS FindKeServiceDescriptorTableWin10(VOID)
{
    ULONG64 msrLstar;
    PUCHAR kiSystemCall64;
    PUCHAR currentPtr;
    ULONG_PTR ssdtOffset;
    PSYSTEM_SERVICE_DESCRIPTOR_TABLE foundTable = NULL;
    
    // Чтение MSR IA32_LSTAR (0xC0000082) - адрес KiSystemCall64
    msrLstar = __readmsr(0xC0000082);
    kiSystemCall64 = (PUCHAR)msrLstar;
    
    DbgPrint("SsdtMon: KiSystemCall64 at 0x%p\n", kiSystemCall64);
    
    // Поиск сигнатуры: 4C 8D 15 XX XX XX XX (lea r10, [rip+offset])
    for (currentPtr = kiSystemCall64 - 0x2000; currentPtr < kiSystemCall64; currentPtr++) {
        if (*(currentPtr + 0) == 0x4C && *(currentPtr + 1) == 0x8D && 
            *(currentPtr + 2) == 0x15) {
            
            ssdtOffset = *(LONG*)(currentPtr + 3);
            foundTable = (PSYSTEM_SERVICE_DESCRIPTOR_TABLE)(currentPtr + 7 + ssdtOffset);
            break;
        }
    }
    
    if (!foundTable) {
        return STATUS_NOT_FOUND;
    }
    
    KeServiceDescriptorTable = foundTable;
    KiServiceTable = (ULONG64*)foundTable->ServiceTableBase;
    KiServiceLimit = foundTable->NumberOfServices;
    
    if (!KiServiceTable || KiServiceLimit == 0) {
        return STATUS_UNSUCCESSFUL;
    }
    
    DbgPrint("SsdtMon: SSDT found at 0x%p, %d services\n", KiServiceTable, KiServiceLimit);
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    CreateMdlForSSDT - создание MDL для безопасной записи в SSDT
    Современный метод вместо отключения CR0.WP
----------------------------------------------------------------------------*/
NTSTATUS CreateMdlForSSDT(VOID)
{
    PMDL mdl;
    PVOID mappedAddress;
    SIZE_T tableSize;
    
    if (!KiServiceTable) {
        return STATUS_INVALID_PARAMETER;
    }
    
    tableSize = KiServiceLimit * sizeof(ULONG64);
    
    mdl = IoAllocateMdl(KiServiceTable, (ULONG)tableSize, FALSE, FALSE, NULL);
    if (!mdl) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    MmBuildMdlForNonPagedPool(mdl);
    mdl->MdlFlags |= MDL_MAPPED_TO_SYSTEM_VA;
    
    mappedAddress = MmMapLockedPagesSpecifyCache(
        mdl, KernelMode, MmCached, NULL, FALSE, NormalPagePriority);
    
    if (!mappedAddress) {
        IoFreeMdl(mdl);
        return STATUS_UNSUCCESSFUL;
    }
    
    KiServiceTable = (ULONG64*)mappedAddress;
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    SaveGoldenCopy - сохранение эталонной копии SSDT (ТЗ раздел 4.2)
----------------------------------------------------------------------------*/
NTSTATUS SaveGoldenCopy(VOID)
{
    ULONG i;
    
    GoldenAddressTable = (ULONG64*)ExAllocatePoolWithTag(
        NonPagedPoolNx, KiServiceLimit * sizeof(ULONG64), 'tdSS');
    
    if (!GoldenAddressTable) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    HookTable = (ULONG64*)ExAllocatePoolWithTag(
        NonPagedPoolNx, KiServiceLimit * sizeof(ULONG64), 'tdSS');
    
    if (!HookTable) {
        ExFreePoolWithTag(GoldenAddressTable, 'tdSS');
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    RtlZeroMemory(HookTable, KiServiceLimit * sizeof(ULONG64));
    
    for (i = 0; i < KiServiceLimit; i++) {
        GoldenAddressTable[i] = KiServiceTable[i];
    }
    
    DbgPrint("SsdtMon: Golden copy saved (%d entries)\n", KiServiceLimit);
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
    RestoreOriginalSSDT - восстановление SSDT из эталона (ТЗ раздел 2.4 use-case 3)
----------------------------------------------------------------------------*/
NTSTATUS RestoreOriginalSSDT(VOID)
{
    ULONG i;
    
    if (!KiServiceTable || !GoldenAddressTable) {
        return STATUS_INVALID_PARAMETER;
    }
    
    for (i = 0; i < KiServiceLimit; i++) {
        if (KiServiceTable[i] != GoldenAddressTable[i]) {
            KiServiceTable[i] = GoldenAddressTable[i];
            HookTable[i] = 0;
            DbgPrint("SsdtMon: Restored index 0x%X\n", i);
        }
    }
    
    return STATUS_SUCCESS;
}