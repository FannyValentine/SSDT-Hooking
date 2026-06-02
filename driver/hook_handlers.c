/*============================================================================
    hook_handlers.c - функции-обработчики для перехвата системных вызовов
    Соответствует ТЗ: разделы 2.3, 2.4 (use-case 1, 2, 4)
============================================================================*/

#include <ntddk.h>
#include "common.h"

// Внешние функции из hook_mgr.c
extern ULONG IncrementHookCallCount(ULONG ServiceIndex);
extern BOOLEAN IsWhitelisted(ULONG ServiceIndex);
extern NTSTATUS SetHookPolicy(ULONG ServiceIndex, ULONG PolicyFlags);

// Внешние функции для логирования
extern NTSTATUS LogEvent(ULONG ServiceIndex, UCHAR ChangeType, ULONG64 Expected, ULONG64 Actual);

/*============================================================================
    Структуры параметров системных вызовов (упрощённые)
    Для реального использования нужно определять точные структуры
    через reverse engineering или PDB символы
============================================================================*/

// Параметры NtOpenProcess (индекс 0x7F в некоторых версиях Windows)
typedef struct _NtOpenProcess_PARAMETERS {
    PHANDLE ProcessHandle;
    ACCESS_MASK DesiredAccess;
    POBJECT_ATTRIBUTES ObjectAttributes;
    PCLIENT_ID ClientId;
} NtOpenProcess_PARAMETERS, *PNtOpenProcess_PARAMETERS;

// Параметры NtCreateFile (индекс 0x3F в некоторых версиях Windows)
typedef struct _NtCreateFile_PARAMETERS {
    PHANDLE FileHandle;
    ACCESS_MASK DesiredAccess;
    POBJECT_ATTRIBUTES ObjectAttributes;
    PIO_STATUS_BLOCK IoStatusBlock;
    PLARGE_INTEGER AllocationSize;
    ULONG FileAttributes;
    ULONG ShareAccess;
    ULONG CreateDisposition;
    ULONG CreateOptions;
    PVOID EaBuffer;
    ULONG EaLength;
} NtCreateFile_PARAMETERS, *PNtCreateFile_PARAMETERS;

// Параметры NtQuerySystemInformation
typedef struct _NtQuerySystemInformation_PARAMETERS {
    ULONG SystemInformationClass;
    PVOID SystemInformation;
    ULONG SystemInformationLength;
    PULONG ReturnLength;
} NtQuerySystemInformation_PARAMETERS, *PNtQuerySystemInformation_PARAMETERS;

// Параметры NtTerminateProcess
typedef struct _NtTerminateProcess_PARAMETERS {
    HANDLE ProcessHandle;
    NTSTATUS ExitStatus;
} NtTerminateProcess_PARAMETERS, *PNtTerminateProcess_PARAMETERS;

/*============================================================================
    Вспомогательные функции для обработчиков
============================================================================*/

// Проверка, является ли процесс защищённым (например, системные процессы)
BOOLEAN IsProtectedProcess(HANDLE ProcessId)
{
    // Простой пример: блокируем завершение системных процессов
    // В реальном драйвере нужно проверять через PsLookupProcessByProcessId
    if (ProcessId == (HANDLE)4 || ProcessId == (HANDLE)0) {
        return TRUE;  // System process or Idle process
    }
    return FALSE;
}

// Получение имени процесса по ID (упрощённая версия)
NTSTATUS GetProcessNameById(HANDLE ProcessId, PCHAR Buffer, ULONG BufferSize)
{
    PEPROCESS process;
    NTSTATUS status;
    
    status = PsLookupProcessByProcessId(ProcessId, &process);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    // Получаем имя образа процесса
    if (Buffer && BufferSize > 0) {
        PUNICODE_STRING imageName = PsGetProcessImageFileName(process);
        if (imageName && imageName->Buffer) {
            // Конвертация UNICODE_STRING в ANSI (упрощённо)
            ULONG i;
            for (i = 0; i < min(imageName->Length / 2, BufferSize - 1); i++) {
                Buffer[i] = (CHAR)imageName->Buffer[i];
            }
            Buffer[i] = '\0';
        } else {
            RtlStringCbCopyA(Buffer, BufferSize, "Unknown");
        }
    }
    
    ObDereferenceObject(process);
    return STATUS_SUCCESS;
}

/*============================================================================
    ОСНОВНОЙ ОБРАБОТЧИК ХУКА (универсальный)
    Перехватывает системный вызов и применяет политики
============================================================================*/

NTSTATUS MyHookHandler(
    ULONG ServiceIndex,
    PVOID SystemCallArguments,
    PVOID OriginalFunction)
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG callCount;
    
    DbgPrint("SsdtMon: Hook triggered for index 0x%X\n", ServiceIndex);
    
    // ========================================================================
    // 1. Логирование вызова (соответствует ТЗ раздел 2.4 use-case 2)
    // ========================================================================
    callCount = IncrementHookCallCount(ServiceIndex);
    DbgPrint("SsdtMon: Syscall index 0x%X called %d times\n", ServiceIndex, callCount);
    
    // Логируем сам факт вызова (по желанию)
    LogEvent(ServiceIndex, CHANGE_TYPE_ADDRESS, (ULONG64)OriginalFunction, (ULONG64)MyHookHandler);
    
    // ========================================================================
    // 2. Проверка белого списка (соответствует ТЗ раздел 2.4 use-case 1)
    // ========================================================================
    if (!IsWhitelisted(ServiceIndex)) {
        DbgPrint("SsdtMon: [BLOCKED] Syscall index 0x%X - not in whitelist\n", ServiceIndex);
        
        // Логируем блокировку
        DbgPrint("SsdtMon: Blocked syscall index 0x%X\n", ServiceIndex);
        
        // Возвращаем ошибку доступа
        return STATUS_ACCESS_DENIED;
    }
    
    // ========================================================================
    // 3. Специфичная обработка по индексу системного вызова
    // ========================================================================
    switch (ServiceIndex) {
        case 0x7F:  // NtOpenProcess (пример)
        {
            PNtOpenProcess_PARAMETERS params = (PNtOpenProcess_PARAMETERS)SystemCallArguments;
            
            DbgPrint("SsdtMon: NtOpenProcess called - DesiredAccess: 0x%X\n", 
                     params->DesiredAccess);
            
            // Блокируем открытие системных процессов с правами на завершение
            if (params->ClientId && params->ClientId->UniqueProcess) {
                HANDLE pid = params->ClientId->UniqueProcess;
                
                if (IsProtectedProcess(pid)) {
                    DbgPrint("SsdtMon: [BLOCKED] Attempt to open protected process (PID: %p)\n", pid);
                    return STATUS_ACCESS_DENIED;
                }
                
                // Логируем успешное открытие
                DbgPrint("SsdtMon: Process opened (PID: %p)\n", pid);
            }
            
            // Вызов оригинальной функции
            status = ((NTSTATUS (*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PCLIENT_ID))
                     OriginalFunction)(
                         params->ProcessHandle,
                         params->DesiredAccess,
                         params->ObjectAttributes,
                         params->ClientId);
            
            DbgPrint("SsdtMon: NtOpenProcess returned 0x%X\n", status);
            return status;
        }
        
        case 0x3F:  // NtCreateFile (пример)
        {
            PNtCreateFile_PARAMETERS params = (PNtCreateFile_PARAMETERS)SystemCallArguments;
            
            DbgPrint("SsdtMon: NtCreateFile called - CreateDisposition: 0x%X\n", 
                     params->CreateDisposition);
            
            // Блокируем создание файлов в системных директориях (пример политики)
            if (params->ObjectAttributes && params->ObjectAttributes->ObjectName) {
                PUNICODE_STRING fileName = params->ObjectAttributes->ObjectName;
                
                // Проверка на создание файлов в System32
                if (fileName->Length >= 36) {  // "\??\C:\Windows\System32\" ~ 36 символов
                    DbgPrint("SsdtMon: File creation in System32 detected: %wZ\n", fileName);
                    
                    // Блокируем запись в System32
                    // return STATUS_ACCESS_DENIED;
                }
            }
            
            // Вызов оригинальной функции
            status = ((NTSTATUS (*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK,
                                   PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG))
                     OriginalFunction)(
                         params->FileHandle,
                         params->DesiredAccess,
                         params->ObjectAttributes,
                         params->IoStatusBlock,
                         params->AllocationSize,
                         params->FileAttributes,
                         params->ShareAccess,
                         params->CreateDisposition,
                         params->CreateOptions,
                         params->EaBuffer,
                         params->EaLength);
            
            return status;
        }
        
        case 0x23:  // NtTerminateProcess (пример - защита от завершения процессов)
        {
            PNtTerminateProcess_PARAMETERS params = (PNtTerminateProcess_PARAMETERS)SystemCallArguments;
            
            DbgPrint("SsdtMon: NtTerminateProcess called - ExitStatus: 0x%X\n", 
                     params->ExitStatus);
            
            // Блокируем завершение защищённых процессов
            if (params->ProcessHandle && params->ProcessHandle != (HANDLE)-1) {
                HANDLE pid = (HANDLE)PsGetProcessId(PsGetCurrentProcess());
                
                if (IsProtectedProcess(pid)) {
                    DbgPrint("SsdtMon: [BLOCKED] Attempt to terminate protected process (PID: %p)\n", pid);
                    return STATUS_ACCESS_DENIED;
                }
            }
            
            // Вызов оригинальной функции
            status = ((NTSTATUS (*)(HANDLE, NTSTATUS))OriginalFunction)(
                         params->ProcessHandle,
                         params->ExitStatus);
            
            return status;
        }
        
        default:
        {
            // Для всех остальных системных вызовов просто передаём управление оригиналу
            DbgPrint("SsdtMon: Unknown syscall index 0x%X - passing through\n", ServiceIndex);
            
            // Обобщённый вызов для неизвестных системных вызовов
            // В реальном драйвере нужно знать сигнатуру функции
            status = ((NTSTATUS (*)(void))OriginalFunction)();
            return status;
        }
    }
}

/*============================================================================
    СПЕЦИАЛИЗИРОВАННЫЕ ОБРАБОТЧИКИ ДЛЯ РАЗНЫХ USE-CASE
============================================================================*/

/*----------------------------------------------------------------------------
    LogOnlyHookHandler - только логирование (без изменения поведения)
    Соответствует ТЗ: раздел 2.4 use-case 2 (сбор статистики)
----------------------------------------------------------------------------*/
NTSTATUS LogOnlyHookHandler(
    ULONG ServiceIndex,
    PVOID SystemCallArguments,
    PVOID OriginalFunction)
{
    // Логируем вызов с параметрами
    DbgPrint("SsdtMon: [LOG] Syscall 0x%X called\n", ServiceIndex);
    
    // Увеличиваем счётчик вызовов
    IncrementHookCallCount(ServiceIndex);
    
    // Просто передаём управление оригинальной функции
    return ((NTSTATUS (*)(PVOID))OriginalFunction)(SystemCallArguments);
}

/*----------------------------------------------------------------------------
    WhitelistHookHandler - белый список разрешённых вызовов
    Соответствует ТЗ: раздел 2.4 use-case 1 (белый список)
----------------------------------------------------------------------------*/
NTSTATUS WhitelistHookHandler(
    ULONG ServiceIndex,
    PVOID SystemCallArguments,
    PVOID OriginalFunction)
{
    // Проверяем, разрешён ли этот системный вызов
    if (!IsWhitelisted(ServiceIndex)) {
        DbgPrint("SsdtMon: [WHITELIST] Syscall 0x%X blocked (not in whitelist)\n", ServiceIndex);
        return STATUS_ACCESS_DENIED;
    }
    
    DbgPrint("SsdtMon: [WHITELIST] Syscall 0x%X allowed\n", ServiceIndex);
    
    // Вызов оригинальной функции
    return ((NTSTATUS (*)(PVOID))OriginalFunction)(SystemCallArguments);
}

/*----------------------------------------------------------------------------
    SandboxHookHandler - эмуляция вызова в песочнице
    Соответствует ТЗ: раздел 2.4 use-case 4 (эмуляция вызова в песочнице)
----------------------------------------------------------------------------*/
NTSTATUS SandboxHookHandler(
    ULONG ServiceIndex,
    PVOID SystemCallArguments,
    PVOID OriginalFunction)
{
    NTSTATUS realResult;
    NTSTATUS emulatedResult;
    
    DbgPrint("SsdtMon: [SANDBOX] Emulating syscall 0x%X\n", ServiceIndex);
    
    // 1. Логируем параметры перед вызовом
    DbgPrint("SsdtMon: Sandbox - Arguments at 0x%p\n", SystemCallArguments);
    
    // 2. Выполняем реальный вызов (можно в изолированном контексте)
    //    В реальной реализации нужно копировать параметры и выполнять
    //    в другом процессе или с изменёнными привилегиями
    
    __try {
        realResult = ((NTSTATUS (*)(PVOID))OriginalFunction)(SystemCallArguments);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DbgPrint("SsdtMon: Exception in real call! Code: 0x%X\n", GetExceptionCode());
        realResult = GetExceptionCode();
    }
    
    // 3. Эмулируем результат (для тестов)
    //    Можно подменять результат на основе правил
    emulatedResult = realResult;
    
    // Пример эмуляции: всегда возвращать успех для определённых вызовов
    if (ServiceIndex == 0x7F) {  // NtOpenProcess
        // Эмулируем успешное открытие, даже если реальное не удалось
        emulatedResult = STATUS_SUCCESS;
        DbgPrint("SsdtMon: [SANDBOX] Emulated success for NtOpenProcess\n");
    }
    
    // 4. Сравниваем результаты
    if (realResult != emulatedResult) {
        DbgPrint("SsdtMon: [SANDBOX] Result mismatch - Real: 0x%X, Emulated: 0x%X\n",
                 realResult, emulatedResult);
        LogEvent(ServiceIndex, CHANGE_TYPE_ADDRESS, realResult, emulatedResult);
    }
    
    // 5. Возвращаем эмулированный результат
    return emulatedResult;
}

/*----------------------------------------------------------------------------
    ModifyParametersHookHandler - модификация параметров вызова
    Соответствует ТЗ: раздел 2.3 (модификация параметров)
----------------------------------------------------------------------------*/
NTSTATUS ModifyParametersHookHandler(
    ULONG ServiceIndex,
    PVOID SystemCallArguments,
    PVOID OriginalFunction)
{
    DbgPrint("SsdtMon: [MODIFY] Modifying parameters for syscall 0x%X\n", ServiceIndex);
    
    switch (ServiceIndex) {
        case 0x7F:  // NtOpenProcess
        {
            PNtOpenProcess_PARAMETERS params = (PNtOpenProcess_PARAMETERS)SystemCallArguments;
            
            // Пример: ограничиваем права доступа
            ACCESS_MASK originalAccess = params->DesiredAccess;
            
            // Убираем право на завершение процесса (PROCESS_TERMINATE = 0x0001)
            params->DesiredAccess &= ~0x0001;
            
            DbgPrint("SsdtMon: Modified DesiredAccess from 0x%X to 0x%X\n",
                     originalAccess, params->DesiredAccess);
            break;
        }
        
        case 0x3F:  // NtCreateFile
        {
            PNtCreateFile_PARAMETERS params = (PNtCreateFile_PARAMETERS)SystemCallArguments;
            
            // Пример: запрещаем создание файлов (меняем CreateDisposition)
            ULONG originalDisposition = params->CreateDisposition;
            
            // FILE_CREATE = 0x01, заменяем на FILE_OPEN = 0x01 (фактически только открытие)
            if (params->CreateDisposition == 0x01) {  // CREATE
                params->CreateDisposition = 0x01;     // OPEN (может не сработать)
                DbgPrint("SsdtMon: Changed CreateDisposition from CREATE to OPEN\n");
            }
            break;
        }
    }
    
    // Вызов оригинальной функции с модифицированными параметрами
    return ((NTSTATUS (*)(PVOID))OriginalFunction)(SystemCallArguments);
}

/*----------------------------------------------------------------------------
    BlockHookHandler - полная блокировка системного вызова
    Соответствует ТЗ: раздел 2.3 (блокировка вызова)
----------------------------------------------------------------------------*/
NTSTATUS BlockHookHandler(
    ULONG ServiceIndex,
    PVOID SystemCallArguments,
    PVOID OriginalFunction)
{
    UNREFERENCED_PARAMETER(SystemCallArguments);
    UNREFERENCED_PARAMETER(OriginalFunction);
    
    DbgPrint("SsdtMon: [BLOCK] Syscall 0x%X blocked by policy\n", ServiceIndex);
    
    // Логируем блокировку
    LogEvent(ServiceIndex, CHANGE_TYPE_ADDRESS, (ULONG64)OriginalFunction, 0);
    
    // Возвращаем ошибку доступа
    return STATUS_ACCESS_DENIED;
}

/*============================================================================
    Функция для установки конкретного обработчика на индекс
============================================================================*/

NTSTATUS InstallPolicyHandler(
    ULONG ServiceIndex,
    ULONG PolicyType,
    ULONG64 HandlerAddress)
{
    NTSTATUS status;
    
    DbgPrint("SsdtMon: Installing policy handler for index 0x%X (type: %d)\n", 
             ServiceIndex, PolicyType);
    
    // Устанавливаем политику
    status = SetHookPolicy(ServiceIndex, PolicyType);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    // Устанавливаем хук на указанную функцию-обработчик
    status = InstallHook(ServiceIndex, HandlerAddress);
    
    return status;
}

/*============================================================================
    Пример: установка всех обработчиков при инициализации драйвера
============================================================================*/

NTSTATUS InitializeAllHooks(VOID)
{
    NTSTATUS status;
    
    // Установка обработчика с политикой "только логирование"
    status = InstallPolicyHandler(0x3F, 0x01, (ULONG64)LogOnlyHookHandler);
    if (!NT_SUCCESS(status)) {
        DbgPrint("SsdtMon: Failed to install log hook on NtCreateFile\n");
    }
    
    // Установка обработчика с политикой "белый список"
    status = InstallPolicyHandler(0x7F, 0x08, (ULONG64)WhitelistHookHandler);
    if (!NT_SUCCESS(status)) {
        DbgPrint("SsdtMon: Failed to install whitelist hook on NtOpenProcess\n");
    }
    
    // Установка обработчика с политикой "модификация параметров"
    status = InstallPolicyHandler(0x8A, 0x04, (ULONG64)ModifyParametersHookHandler);
    if (!NT_SUCCESS(status)) {
        DbgPrint("SsdtMon: Failed to install modify hook\n");
    }
    
    // Установка обработчика с политикой "полная блокировка"
    status = InstallPolicyHandler(0x23, 0x02, (ULONG64)BlockHookHandler);
    if (!NT_SUCCESS(status)) {
        DbgPrint("SsdtMon: Failed to install block hook on NtTerminateProcess\n");
    }
    
    DbgPrint("SsdtMon: All hooks initialized\n");
    return STATUS_SUCCESS;
}