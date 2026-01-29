#include "MemoryReader.h"

#include <tlhelp32.h>

MemoryReader::MemoryReader() : processHandle(nullptr), processId(0), moduleBase(0) {}

MemoryReader::~MemoryReader() {
    if (processHandle) {
        CloseHandle(processHandle);
    }
}

bool MemoryReader::FindProcess(const std::wstring& processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32 processEntry = {};
    processEntry.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &processEntry)) {
        do {
            if (wcscmp(processName.c_str(), processEntry.szExeFile) == 0) {
                processId = processEntry.th32ProcessID;
                CloseHandle(snapshot);
                return true;
            }
        } while (Process32Next(snapshot, &processEntry));
    }

    CloseHandle(snapshot);
    return false;
}

bool MemoryReader::FindModuleBase(const std::wstring& moduleName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (snapshot == INVALID_HANDLE_VALUE) return false;

    MODULEENTRY32 moduleEntry = {};
    moduleEntry.dwSize = sizeof(MODULEENTRY32);

    if (Module32First(snapshot, &moduleEntry)) {
        do {
            if (wcscmp(moduleName.c_str(), moduleEntry.szModule) == 0) {
                moduleBase = reinterpret_cast<uintptr_t>(moduleEntry.modBaseAddr);
                CloseHandle(snapshot);
                return true;
            }
        } while (Module32Next(snapshot, &moduleEntry));
    }

    CloseHandle(snapshot);
    return false;
}

std::expected<void, MemoryReaderError> MemoryReader::Initialize(const std::wstring& processName) {
    if (processHandle) {
        CloseHandle(processHandle);
        processHandle = nullptr;
    }

    if (!FindProcess(processName)) {
        return std::unexpected(MemoryReaderError::ProcessNotFound);
    }

    processHandle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!processHandle) {
        return std::unexpected(MemoryReaderError::AccessDenied);
    }

    if (!FindModuleBase(processName)) {
        return std::unexpected(MemoryReaderError::ModuleNotFound);
    }

    return {};
}

uintptr_t MemoryReader::GetModuleBase() const {
    return moduleBase;
}

bool MemoryReader::IsInitialized() const {
    return processHandle != nullptr;
}

bool MemoryReader::IsProcessRunning() const {
    if (!processHandle) {
        return false;
    }

    DWORD exitCode = 0;
    if (!GetExitCodeProcess(processHandle, &exitCode)) {
        return false;
    }

    return exitCode == STILL_ACTIVE;
}
