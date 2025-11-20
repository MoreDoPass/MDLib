#pragma once

#include <string>
#include <windows.h>  // Для типа DWORD
#include <cstdint>    // Для uintptr_t

/**
 * @class InjectionManager
 * @brief Статический класс-утилита для выполнения инъекции DLL в другой процесс.
 */
class InjectionManager
{
   public:
    /**
     * @brief Выполняет инъекцию DLL в целевой процесс.
     * @param processId ID процесса, в который будет произведена инъекция.
     * @param dllPath Полный путь к файлу DLL, который необходимо инжектировать.
     * @return Базовый адрес загруженной DLL в памяти целевого процесса в случае успеха.
     *         Возвращает 0 в случае ошибки.
     */
    static uintptr_t Inject(DWORD processId, const std::string& dllPath);
};