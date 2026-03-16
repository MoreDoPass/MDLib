#include "ProcessManager.h"
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <codecvt>
#include <locale>

ProcessManager::LogCallback ProcessManager::s_logger = [](LogLevel level, const std::string &message)
{
    // Логгер по умолчанию, который пишет в консоль
    const char *levelStr =
        (level == LogLevel::CRITICAL) ? "CRITICAL" : (level == LogLevel::WARNING ? "WARNING" : "INFO");
    std::cout << "[ProcessManager] [" << levelStr << "] " << message << std::endl;
};

void ProcessManager::setLogger(LogCallback logger)
{
    if (logger)
    {
        s_logger = logger;
    }
}

BOOL CALLBACK ProcessManager::EnumWindowsCallback(HWND hwnd, LPARAM lParam)
{
    EnumData *data = reinterpret_cast<EnumData *>(lParam);
    if (!data)
        return TRUE;

    if (IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == (HWND) nullptr)
    {
        DWORD windowPid;
        GetWindowThreadProcessId(hwnd, &windowPid);
        if (windowPid == data->pid)
        {
            data->hwnd = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

HWND ProcessManager::findMainWindowHandle(uint32_t pid)
{
    EnumData data{pid, nullptr};
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&data));
    return data.hwnd;
}

std::vector<ProcessInfo> ProcessManager::findProcesses(std::function<bool(const std::wstring &)> filter)
{
    std::vector<ProcessInfo> processes;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE)
    {
        s_logger(LogLevel::CRITICAL, "Failed to get process snapshot. WinAPI Error: " + std::to_string(GetLastError()));
        return processes;
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe))
    {
        do
        {
            // Пропускаем имя процесса через наш фильтр
            if (filter(std::wstring(pe.szExeFile)))
            {
                HWND hwnd = findMainWindowHandle(pe.th32ProcessID);
                if (hwnd)
                {
                    processes.push_back({pe.th32ProcessID, std::wstring(pe.szExeFile), hwnd});
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }
    else if (GetLastError() != ERROR_NO_MORE_FILES)
    {
        s_logger(LogLevel::WARNING, "Process32FirstW failed. WinAPI Error: " + std::to_string(GetLastError()));
    }

    CloseHandle(hSnap);
    s_logger(LogLevel::INFO, "Found " + std::to_string(processes.size()) + " processes with visible main window.");
    return processes;
}

std::vector<ProcessInfo> ProcessManager::findProcessesByName(const std::wstring &processName)
{
    // Вызываем нашу новую универсальную функцию с простым правилом точного совпадения
    return findProcesses([&processName](const std::wstring &name)
                         { return _wcsicmp(name.c_str(), processName.c_str()) == 0; });
}

bool ProcessManager::setProcessWindowTitle(HWND handle, const std::wstring &newTitle)
{
    if (!handle)
    {
        s_logger(LogLevel::WARNING, "Cannot set window title: Invalid window handle (nullptr).");
        return false;
    }

    if (!SetWindowTextW(handle, newTitle.c_str()))
    {
        s_logger(LogLevel::WARNING, "Failed to set window title. WinAPI Error: " + std::to_string(GetLastError()));
        return false;
    }

    // Конвертация wstring в string для лога
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    std::string titleStr = converter.to_bytes(newTitle);

    s_logger(LogLevel::INFO, "Successfully set window title to '" + titleStr + "'.");
    return true;
}
