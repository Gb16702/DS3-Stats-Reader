#include <iostream>
#include <windows.h>
#include <tlhelp32.h>
#include <expected>

enum class DS3Error {
    GameNotFound,
    AccessDenied,
    ModuleNotFound,
    ReadFailed
};


class DS3MemoryReader {
private:
    HANDLE processHandle;
    DWORD processId;
    uintptr_t moduleBase;

public:
    DS3MemoryReader() : processHandle(nullptr), processId(0), moduleBase(0) {}

    ~DS3MemoryReader() {
        if (processHandle) {
            CloseHandle(processHandle);
        }
    }

    std::expected<void, DS3Error> Initialize() {
        if (!FindProcess(L"DarkSoulsIII.exe")) {
            return std::unexpected(DS3Error::GameNotFound);
        }

        processHandle = OpenProcess(PROCESS_VM_READ, FALSE, processId);
        if (!processHandle) {
            return std::unexpected(DS3Error::AccessDenied);
        }

        if (!GetModuleBase(L"DarkSoulsIII.exe")) {
            return std::unexpected(DS3Error::ModuleNotFound);
        }

        return {};
    }
    
    std::expected<int, DS3Error> GetDeathCount() {
        const uintptr_t GAMEDATAMAN_POINTER = 0x047572B8;
        const uintptr_t DEATH_COUNT_OFFSET = 0x98;

        uintptr_t pointerAddress = moduleBase + GAMEDATAMAN_POINTER;

        uintptr_t gameDataManAddress = 0;
        if (!ReadMemory(pointerAddress, gameDataManAddress)) {
            return std::unexpected(DS3Error::ReadFailed);
        }

        uintptr_t deathCountAddress = gameDataManAddress + DEATH_COUNT_OFFSET;

        int deathCount = 0;
        if (!ReadMemory(deathCountAddress, deathCount)) {
            return std::unexpected(DS3Error::ReadFailed);
        }

        return deathCount;
    }

private:
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

    bool GetModuleBase(const std::wstring& moduleName) {
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

    template<typename T>
    bool ReadMemory(uintptr_t address, T& value) {
        SIZE_T bytesRead;
        return ReadProcessMemory(processHandle, reinterpret_cast<LPCVOID>(address),
            &value, sizeof(T), &bytesRead) && bytesRead == sizeof(T);
    }
};

int main() {
    DS3MemoryReader reader;

    auto initializeResult = reader.Initialize();
    if (!initializeResult) {
        switch (initializeResult.error()) {
            case DS3Error::GameNotFound:
                std::cout << "Error: Dark Souls III not found. Make sure the game is running." << std::endl;
                break;
            case DS3Error::AccessDenied:
                std::cout << "Error: Cannot access process. Try running as administrator." << std::endl;
                break;
            case DS3Error::ModuleNotFound:
                std::cout << "Error: Cannot find game module." << std::endl;
                break;
        }

        system("pause");
        return 1;
    }

    auto deathCountResult = reader.GetDeathCount();
    if (deathCountResult.has_value()) {
        std::cout << "Deaths: " << *deathCountResult << std::endl;
    } else {
        switch (deathCountResult.error()) {
        case DS3Error::ReadFailed:
            std::cout << "Error: Failed to read  death count from memory" << std::endl;
            break;
        }
    }

    system("pause");

    return 0;
}
