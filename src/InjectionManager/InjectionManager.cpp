#include "InjectionManager.h"
#include <stdexcept>
#include <vector>

uintptr_t InjectionManager::Inject(DWORD processId, const std::string& dllPath)
{
    // ШАГ 0: Получаем полный абсолютный путь к DLL.
    std::vector<char> fullPathBuff(MAX_PATH);
    if (GetFullPathNameA(dllPath.c_str(), fullPathBuff.size(), fullPathBuff.data(), nullptr) == 0)
    {
        return 0;  // Возвращаем 0 при ошибке
    }
    std::string fullDllPath = fullPathBuff.data();

    HANDLE hProcess = NULL;
    LPVOID pRemoteBuf = NULL;
    HANDLE hThread = NULL;
    uintptr_t injectedDllBaseAddress = 0;  // Здесь будет результат

    // ШАГ 1: Открываем процесс
    hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, processId
    );
    if (hProcess == NULL)
    {
        return 0;
    }

    // ШАГ 2: Выделяем память
    pRemoteBuf = VirtualAllocEx(hProcess, NULL, fullDllPath.size() + 1, MEM_COMMIT, PAGE_READWRITE);
    if (pRemoteBuf == NULL)
    {
        CloseHandle(hProcess);
        return 0;
    }

    // ШАГ 3: Записываем путь
    if (!WriteProcessMemory(hProcess, pRemoteBuf, fullDllPath.c_str(), fullDllPath.size() + 1, NULL))
    {
        VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 0;
    }

    // ШАГ 4: Создаем удаленный поток
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    FARPROC pLoadLibrary = GetProcAddress(hKernel32, "LoadLibraryA");

    hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, pRemoteBuf, 0, NULL);
    if (hThread == NULL)
    {
        VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 0;
    }

    // ШАГ 5: Ожидаем завершения потока LoadLibraryA.
    // Это ключевой шаг - мы ждем, пока DLL загрузится (или не загрузится).
    WaitForSingleObject(hThread, INFINITE);

    // ШАГ 6: Получаем код завершения потока.
    // Для потока, запустившего LoadLibraryA, код завершения - это HMODULE (базовый адрес) загруженной DLL!
    DWORD exitCode = 0;
    if (GetExitCodeThread(hThread, &exitCode))
    {
        // Если exitCode не NULL, значит LoadLibraryA успешно вернула адрес.
        if (exitCode != 0)
        {
            injectedDllBaseAddress = (uintptr_t)exitCode;
        }
    }

    // ШАГ 7: Очищаем за собой все ресурсы.
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return injectedDllBaseAddress;
}