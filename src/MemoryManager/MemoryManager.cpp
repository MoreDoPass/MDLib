#include "MemoryManager.h"
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <codecvt>
#include <locale>

MemoryManager::LogCallback MemoryManager::s_logger = [](LogLevel level, const std::string& message)
{
    const char* levelStr =
        (level == LogLevel::CRITICAL) ? "CRITICAL" : (level == LogLevel::WARNING ? "WARNING" : "INFO");
    std::cout << "[MemoryManager] [" << levelStr << "] " << message << std::endl;
};

void MemoryManager::setLogger(LogCallback logger)
{
    if (logger) s_logger = logger;
}

// Вспомогательная функция для красивого вывода адресов
std::string MemoryManager::to_hex(uintptr_t val)
{
    std::stringstream stream;
    stream << "0x" << std::hex << val;
    return stream.str();
}

MemoryManager::MemoryManager()
{
    s_logger(LogLevel::INFO, "MemoryManager created.");
}

MemoryManager::~MemoryManager()
{
    closeProcess();
    s_logger(LogLevel::INFO, "MemoryManager destroyed.");
}

bool MemoryManager::openProcess(DWORD pid, const std::wstring& mainModuleName)
{
    closeProcess();
    m_processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!m_processHandle)
    {
        s_logger(
            LogLevel::CRITICAL,
            "Failed to open process PID: " + std::to_string(pid) + ". WinAPI Error: " + std::to_string(GetLastError())
        );
        m_pid = 0;
        return false;
    }
    m_pid = pid;
    m_mainModuleName = mainModuleName;
    m_mainModuleBaseAddress = 0;

    // Конвертируем wstring в string для лога
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    std::string moduleNameStr = converter.to_bytes(mainModuleName);

    s_logger(LogLevel::INFO, "Process opened. PID: " + std::to_string(pid) + ", Module: '" + moduleNameStr + "'");
    return true;
}

void MemoryManager::closeProcess()
{
    if (m_processHandle)
    {
        if (!CloseHandle(m_processHandle))
        {
            s_logger(
                LogLevel::CRITICAL,
                "Error closing process handle for PID: " + std::to_string(m_pid) +
                    ". WinAPI Error: " + std::to_string(GetLastError())
            );
        }
        else
        {
            s_logger(LogLevel::INFO, "Process closed. PID: " + std::to_string(m_pid));
        }
        m_processHandle = nullptr;
        m_pid = 0;
        m_mainModuleName.clear();
        m_mainModuleBaseAddress = 0;
    }
}

bool MemoryManager::isProcessOpen() const
{
    return m_processHandle != nullptr;
}

std::optional<DWORD> MemoryManager::pid() const
{
    if (m_processHandle) return m_pid;
    return std::nullopt;
}

uintptr_t MemoryManager::getMainModuleBaseAddress()
{
    if (m_mainModuleBaseAddress != 0)
    {
        return m_mainModuleBaseAddress;
    }

    m_mainModuleBaseAddress = findModuleBaseAddress(m_mainModuleName);
    return m_mainModuleBaseAddress;
}

uintptr_t MemoryManager::findModuleBaseAddress(const std::wstring& moduleName)
{
    if (!m_processHandle) return 0;

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_pid);
    if (hSnap == INVALID_HANDLE_VALUE)
    {
        s_logger(
            LogLevel::CRITICAL, "Failed to create module snapshot. WinAPI Error: " + std::to_string(GetLastError())
        );
        return 0;
    }

    MODULEENTRY32W me32;
    me32.dwSize = sizeof(MODULEENTRY32W);
    uintptr_t baseAddress = 0;

    if (Module32FirstW(hSnap, &me32))
    {
        do
        {
            if (_wcsicmp(me32.szModule, moduleName.c_str()) == 0)
            {
                baseAddress = reinterpret_cast<uintptr_t>(me32.modBaseAddr);
                break;
            }
        } while (Module32NextW(hSnap, &me32));
    }
    else if (GetLastError() != ERROR_NO_MORE_FILES)
    {
        s_logger(LogLevel::WARNING, "Module32FirstW failed. WinAPI Error: " + std::to_string(GetLastError()));
    }

    CloseHandle(hSnap);

    if (baseAddress == 0)
    {
        using convert_type = std::codecvt_utf8<wchar_t>;
        std::wstring_convert<convert_type, wchar_t> converter;
        std::string moduleNameStr = converter.to_bytes(moduleName);
        s_logger(LogLevel::WARNING, "Failed to find base address for module: '" + moduleNameStr + "'");
    }
    return baseAddress;
}

void* MemoryManager::allocMemory(size_t size, DWORD protection)
{
    if (!m_processHandle)
    {
        s_logger(LogLevel::CRITICAL, "Attempt to allocate memory while process is not open!");
        return nullptr;
    }
    void* remoteAddr = VirtualAllocEx(m_processHandle, nullptr, size, MEM_COMMIT | MEM_RESERVE, protection);
    if (!remoteAddr)
    {
        s_logger(
            LogLevel::CRITICAL,
            "Failed to allocate memory. Size: " + std::to_string(size) +
                ". WinAPI Error: " + std::to_string(GetLastError())
        );
        return nullptr;
    }
    s_logger(
        LogLevel::INFO,
        "Memory allocated at " + to_hex(reinterpret_cast<uintptr_t>(remoteAddr)) + ", size: " + std::to_string(size)
    );
    return remoteAddr;
}

bool MemoryManager::freeMemory(void* address)
{
    if (!m_processHandle)
    {
        s_logger(LogLevel::CRITICAL, "Attempt to free memory while process is not open!");
        return false;
    }
    if (!address)
    {
        s_logger(LogLevel::CRITICAL, "Attempt to free a nullptr!");
        return false;
    }
    if (!VirtualFreeEx(m_processHandle, address, 0, MEM_RELEASE))
    {
        s_logger(
            LogLevel::CRITICAL,
            "Failed to free memory at " + to_hex(reinterpret_cast<uintptr_t>(address)) +
                ". WinAPI Error: " + std::to_string(GetLastError())
        );
        return false;
    }
    s_logger(LogLevel::INFO, "Memory freed at " + to_hex(reinterpret_cast<uintptr_t>(address)));
    return true;
}

bool MemoryManager::changeMemoryProtection(void* address, size_t size, DWORD newProtection, DWORD* oldProtection)
{
    if (!m_processHandle)
    {
        s_logger(LogLevel::CRITICAL, "Attempt to change memory protection while process is not open!");
        return false;
    }
    if (!address || size == 0)
    {
        s_logger(LogLevel::CRITICAL, "Invalid parameters for changing memory protection.");
        return false;
    }
    DWORD oldProt = 0;
    if (!VirtualProtectEx(m_processHandle, address, size, newProtection, &oldProt))
    {
        s_logger(
            LogLevel::CRITICAL,
            "Failed to change memory protection at " + to_hex(reinterpret_cast<uintptr_t>(address)) +
                ". WinAPI Error: " + std::to_string(GetLastError())
        );
        return false;
    }
    if (oldProtection) *oldProtection = oldProt;
    s_logger(
        LogLevel::INFO,
        "Memory protection changed at " + to_hex(reinterpret_cast<uintptr_t>(address)) +
            ", new protection: " + std::to_string(newProtection)
    );
    return true;
}
