#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <functional>
#include <windows.h>

/**
 * @brief Структура с информацией о процессе.
 */
struct ProcessInfo
{
    uint32_t pid;      ///< Идентификатор процесса
    std::wstring name; ///< Имя процесса

    /**
     * @brief Хендл главного окна процесса (в WinAPI это HWND).
     * @details Мы используем void* для совместимости и чтобы не включать <windows.h> в этот заголовок.
     *          Будет равен nullptr, если главное окно не найдено.
     */
    HWND mainWindowHandle = nullptr;
};

/**
 * @brief Структура с информацией о запущенном нами процессе.
 */
struct LaunchedProcessInfo
{
    uint32_t pid = 0;          ///< Идентификатор процесса
    HANDLE hProcess = nullptr; ///< Хендл процесса (нужно закрыть после использования)
    HANDLE hThread = nullptr;  ///< Хендл главного потока (нужен для ResumeThread)
    bool success = false;      ///< Флаг успешного запуска
};

/**
 * @brief Класс для поиска процессов по имени.
 * @details Использует WinAPI для поиска процессов. Только статические методы.
 */
class ProcessManager
{
public:
    enum class LogLevel
    {
        INFO,
        WARNING,
        CRITICAL
    };
    using LogCallback = std::function<void(LogLevel level, const std::string &message)>;
    static void setLogger(LogCallback logger);

    /**
     * @brief Найти процессы, используя кастомный фильтр.
     * @param filter Функция, которая принимает имя процесса и возвращает true, если процесс подходит.
     * @return Вектор структур ProcessInfo
     */
    static std::vector<ProcessInfo> findProcesses(std::function<bool(const std::wstring &)> filter);

    /**
     * @brief Найти все процессы с заданным именем.
     * @param processName Имя процесса (например, L"run.exe")
     * @return Вектор структур ProcessInfo
     */
    static std::vector<ProcessInfo> findProcessesByName(const std::wstring &processName);

    /**
     * @brief Установить новый заголовок окна для заданного хендла.
     * @param handle Хендл окна (WinAPI HWND, передается как void*)
     * @param newTitle Новый заголовок окна
     * @return true в случае успеха, false в противном случае.
     */
    static bool setProcessWindowTitle(HWND handle, const std::wstring &newTitle);

    /**
     * @brief Запускает новый процесс (опционально в замороженном состоянии).
     * @param exePath Полный путь к исполняемому файлу (например, L"D:\\Games\\Sirus\\run.exe").
     * @param args Аргументы командной строки (например, L"-nosound").
     * @param suspended Если true, процесс будет создан с флагом CREATE_SUSPENDED.
     * @return Структура LaunchedProcessInfo. В случае успеха success == true.
     */
    static LaunchedProcessInfo launchProcess(const std::wstring &exePath, const std::wstring &args, bool suspended = false);

    /**
     * @brief Возобновляет работу приостановленного потока.
     * @param hThread Хендл потока, полученный из launchProcess.
     * @return true в случае успеха.
     */
    static bool resumeThread(HANDLE hThread);

private:
    ProcessManager() = delete; ///< Запретить создание экземпляра

    /**
     * @brief Вспомогательная структура для передачи данных в callback-функцию EnumWindows.
     */
    struct EnumData
    {
        uint32_t pid;        ///< Идентификатор процесса, для которого ищем окно
        HWND hwnd = nullptr; ///< Сюда будет записан хендл найденного окна (HWND)
    };

    /**
     * @brief Callback-функция для WinAPI EnumWindows. Вызывается для каждого окна верхнего уровня.
     * @param hwnd Хендл текущего перечисляемого окна.
     * @param lParam Указатель на нашу структуру EnumData.
     * @return TRUE для продолжения перечисления, FALSE для остановки.
     */
    static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam);

    static HWND findMainWindowHandle(uint32_t pid);
    static LogCallback s_logger;
};
