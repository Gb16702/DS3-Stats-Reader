#include <iostream>
#include <windows.h>
#include <tlhelp32.h>
#include <expected>

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

    bool FindProcess(const std::wstring& processName) {
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

    bool FindModuleBase(const std::wstring& moduleName) {
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


public: 
    MemoryReader() : processHandle(nullptr), processId(0), moduleBase(0) {}

    MemoryReader(const MemoryReader&) = delete;
    MemoryReader& operator=(const MemoryReader&) = delete;

    ~MemoryReader() {
        if (processHandle) {
            CloseHandle(processHandle);
        }
    }

    template<typename T>
    bool ReadMemory(uintptr_t address, T& value) {
        if (!processHandle) {
            return false;
        }

        SIZE_T bytesRead = 0;
        return ReadProcessMemory(processHandle, reinterpret_cast<LPCVOID>(address),
            &value, sizeof(T), &bytesRead) && bytesRead == sizeof(T);
    }

    std::expected<void, MemoryReaderError> Initialize(const std::wstring& processName) {
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

    uintptr_t GetModuleBase() const {
        return moduleBase;
    }
};

class DS3DeathCounter {
private:
    MemoryReader reader;

    static constexpr uintptr_t GAMEDATAMAN_POINTER = 0x047572B8;
    static constexpr uintptr_t DEATH_COUNT_OFFSET = 0x98;

    static constexpr wchar_t PROCESS_NAME[] = L"DarkSoulsIII.exe";

public:
    std::expected<void, MemoryReaderError> Initialize() {
        return reader.Initialize(PROCESS_NAME);
    }

    std::expected<int, MemoryReaderError> GetDeathCount() {
        uintptr_t pointerAddress = reader.GetModuleBase() + GAMEDATAMAN_POINTER;

        uintptr_t gameDataManAddress = 0;
        if (!reader.ReadMemory(pointerAddress, gameDataManAddress)) {
            return std::unexpected(MemoryReaderError::ReadFailed);
        }

        if (gameDataManAddress == 0) {
            return std::unexpected(MemoryReaderError::ReadFailed);
        }

        uintptr_t deathCountAddress = gameDataManAddress + DEATH_COUNT_OFFSET;

        int deathCount = 0;
        if (!reader.ReadMemory(deathCountAddress, deathCount)) {
            return std::unexpected(MemoryReaderError::ReadFailed);
        }

        return deathCount;
    }
};

int main() {
    DS3DeathCounter counter;

    auto initializeResult = counter.Initialize();
    if (!initializeResult) {
        switch (initializeResult.error()) {
            case MemoryReaderError::ProcessNotFound:
                std::cout << "Error: Dark Souls III not found. Make sure the game is running." << std::endl;
                break;
            case MemoryReaderError::AccessDenied:
                std::cout << "Error: Cannot access process. Try running as administrator." << std::endl;
                break;
            case MemoryReaderError::ModuleNotFound:
                std::cout << "Error: Cannot find game module." << std::endl;
                break;
        }

        system("pause");
        return 1;
    }

    auto deathCountResult = counter.GetDeathCount();
    if (deathCountResult.has_value()) {
        std::cout << "Deaths: " << *deathCountResult << std::endl;
    } else {
        switch (deathCountResult.error()) {
        case MemoryReaderError::ReadFailed:
            std::cout << "Error: Failed to read death count from memory" << std::endl;
            break;
        }
    }

    system("pause");

    return 0;
}
