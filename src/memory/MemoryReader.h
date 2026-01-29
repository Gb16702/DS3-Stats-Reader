#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <expected>
#include <string>

enum class MemoryReaderError {
    ProcessNotFound,
    AccessDenied,
    ModuleNotFound,
    ReadFailed
};

class MemoryReader {
private:
    HANDLE processHandle;
    DWORD processId;
    uintptr_t moduleBase;

    bool FindProcess(const std::wstring& processName);
    bool FindModuleBase(const std::wstring& moduleName);

public:
    MemoryReader();

    MemoryReader(const MemoryReader&) = delete;
    MemoryReader& operator=(const MemoryReader&) = delete;

    ~MemoryReader();

    template<typename T>
    bool ReadMemory(uintptr_t address, T& value) {
        if (!processHandle) {
            return false;
        }

        SIZE_T bytesRead = 0;
        return ReadProcessMemory(processHandle, reinterpret_cast<LPCVOID>(address),
            &value, sizeof(T), &bytesRead) && bytesRead == sizeof(T);
    }

    std::expected<void, MemoryReaderError> Initialize(const std::wstring& processName);
    uintptr_t GetModuleBase() const;
    bool IsInitialized() const;
    bool IsProcessRunning() const;
};
