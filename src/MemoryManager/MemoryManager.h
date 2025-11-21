#pragma once

#include <windows.h>
#include <optional>
#include <string>  // Для std::wstring
#include <functional>
#include <cstdint>  // Для uintptr_t

/**
 * @brief Класс для работы с памятью внешнего процесса (например, WoW)
 *
 * Позволяет открывать процесс по PID, читать и писать память, логировать действия.
 */
class MemoryManager
{
   public:
    enum class LogLevel
    {
        INFO,
        WARNING,
        CRITICAL
    };

    using LogCallback = std::function<void(LogLevel level, const std::string& message)>;
    static void setLogger(LogCallback logger);

    MemoryManager();
    ~MemoryManager();

    /**
     * @brief Открыть процесс по PID и имени его главного модуля.
     * @param pid Идентификатор процесса.
     * @param mainModuleName Имя главного исполняемого файла (например, L"run.exe").
     * @return true, если процесс успешно открыт.
     */
    bool openProcess(DWORD pid, const std::wstring& mainModuleName);

    /**
     * @brief Закрыть процесс (если был открыт)
     */
    void closeProcess();

    /**
     * @brief Проверить, открыт ли процесс
     * @return true, если процесс открыт
     */
    bool isProcessOpen() const;

    /**
     * @brief Получить PID открытого процесса
     * @return PID или std::nullopt, если процесс не открыт
     */
    std::optional<DWORD> pid() const;

    /**
     * @brief Получить базовый адрес главного модуля текущего процесса.
     * @return Базовый адрес или 0 при ошибке.
     */
    uintptr_t getMainModuleBaseAddress();

    /**
     * @brief Универсальный шаблонный метод для чтения значения любого типа из памяти процесса.
     * @tparam T Тип данных (int, float, double, структура и т.д.)
     * @param address Абсолютный адрес в памяти процесса
     * @param value Ссылка, куда будет записан результат
     * @return true, если чтение успешно
     */
    template <typename T>
    bool readMemory(uintptr_t address, T& value) const  // <-- Добавляем const
    {
        if (!m_processHandle)
        {
            // Не логируем здесь, чтобы не спамить при частых неудачных чтениях.
            return false;
        }
        SIZE_T bytesRead = 0;
        BOOL result =
            ReadProcessMemory(m_processHandle, reinterpret_cast<LPCVOID>(address), &value, sizeof(T), &bytesRead);
        return (result && bytesRead == sizeof(T));
    }

    /**
     * @brief Метод для чтения строки (char-массива) из памяти процесса.
     * @param address Абсолютный адрес в памяти процесса
     * @param buffer Указатель на буфер, куда будет записана строка
     * @param size Размер буфера (количество байт для чтения)
     * @return true, если чтение успешно
     */
    bool readMemory(uintptr_t address, char* buffer, size_t size) const
    {
        // Убираем try-catch, он здесь не нужен.
        if (!m_processHandle) return false;
        SIZE_T bytesRead = 0;
        BOOL result = ReadProcessMemory(m_processHandle, reinterpret_cast<LPCVOID>(address), buffer, size, &bytesRead);
        // Не логируем неудачное чтение, так как это очень частая операция.
        return (result && bytesRead == size);
    }

    /**
     * @brief Универсальный шаблонный метод для записи значения любого типа в память процесса.
     * @tparam T Тип данных (int, float, double, структура и т.д.)
     * @param address Абсолютный адрес в памяти процесса
     * @param value Значение для записи
     * @return true, если запись успешна
     */
    template <typename T>
    bool writeMemory(uintptr_t address, const T& value)
    {
        if (!m_processHandle)
        {
            s_logger(LogLevel::CRITICAL, "Attempt to write memory while process is not open!");
            return false;
        }
        SIZE_T bytesWritten = 0;
        BOOL result =
            WriteProcessMemory(m_processHandle, reinterpret_cast<LPVOID>(address), &value, sizeof(T), &bytesWritten);
        if (!result || bytesWritten != sizeof(T))
        {
            s_logger(
                LogLevel::WARNING,
                "Failed to write memory at 0x" + to_hex(address) + ". WinAPI Error: " + std::to_string(GetLastError())
            );
            return false;
        }
        return true;
    }

    /**
     * @brief Метод для записи массива байт (например, строки) в память процесса.
     */
    bool writeMemory(uintptr_t address, const char* buffer, size_t size)
    {
        if (!m_processHandle)
        {
            s_logger(LogLevel::CRITICAL, "Attempt to write memory string while process is not open!");
            return false;
        }
        SIZE_T bytesWritten = 0;
        BOOL result =
            WriteProcessMemory(m_processHandle, reinterpret_cast<LPVOID>(address), buffer, size, &bytesWritten);
        if (!result || bytesWritten != size)
        {
            s_logger(
                LogLevel::WARNING,
                "Failed to write memory string at " + to_hex(address) +
                    ". WinAPI Error: " + std::to_string(GetLastError())
            );
            return false;
        }
        return true;
    }

    /**
     * @brief Выделить память во внешнем процессе
     * @param size Размер в байтах
     * @param protection Защита памяти (по умолчанию RWX)
     * @return Указатель на выделенную память или nullptr при ошибке
     */
    void* allocMemory(size_t size, DWORD protection = PAGE_EXECUTE_READWRITE);

    /**
     * @brief Освободить ранее выделенную память во внешнем процессе
     * @param address Указатель на память
     * @return true, если память успешно освобождена
     */
    bool freeMemory(void* address);

    /**
     * @brief Изменить защиту участка памяти во внешнем процессе
     * @param address Указатель на память
     * @param size Размер участка
     * @param newProtection Новая защита (например, PAGE_EXECUTE_READWRITE)
     * @param oldProtection [out] Предыдущая защита (опционально)
     * @return true, если успешно
     */
    bool changeMemoryProtection(void* address, size_t size, DWORD newProtection, DWORD* oldProtection = nullptr);

   private:
    /**
     * @brief Находит базовый адрес модуля в процессе. Вспомогательный метод.
     * @param moduleName Имя модуля для поиска.
     * @return Базовый адрес или 0.
     */
    uintptr_t findModuleBaseAddress(const std::wstring& moduleName);

    static std::string to_hex(uintptr_t val);

    static LogCallback s_logger;

   protected:
    HANDLE m_processHandle = nullptr;
    DWORD m_pid = 0;
    std::wstring m_mainModuleName;          ///< Имя главного модуля (run.exe, Wow.exe)
    uintptr_t m_mainModuleBaseAddress = 0;  ///< Кэшированный базовый адрес главного модуля
};
