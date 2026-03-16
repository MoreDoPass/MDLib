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
