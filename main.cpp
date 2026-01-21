#include "httplib.h"
#include "json.hpp"

#include <tlhelp32.h>
#include <windows.h>

#include <chrono>
#include <expected>
#include <iostream>

using json = nlohmann::json;

constexpr const char* APP_VERSION = "1.0.0";
constexpr const char* ALLOWED_ORIGIN = "http://localhost:5173";
constexpr int SERVER_PORT = 3000;

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

void setupRoutes(httplib::Server& server, DS3DeathCounter& counter, std::chrono::steady_clock::time_point startTime) {
    server.set_post_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        auto origin = req.get_header_value("Origin");
        if (ALLOWED_ORIGIN == origin) {
            res.set_header("Access-Control-Allow-Origin", origin);
        }
    });

    server.Get("/health", [startTime](const httplib::Request& req, httplib::Response& res) {
        auto now = std::chrono::steady_clock::now();
        auto uptimeSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

        json response = {
            {"status", "ok"},
            {"version", APP_VERSION},
            {"uptime", uptimeSeconds}
        };

        res.set_content(response.dump(), "application/json");
    });
    
    server.Get("/api/stats", [&](const httplib::Request& req, httplib::Response& res) {        
        auto initializeResult = counter.Initialize();
        if (!initializeResult) {
            json response = {
                {"success", false},
                {"error", {
                    {"code", "GAME_NOT_RUNNING"},
                    {"message", "Make sure Dark Souls III is running"}
                }}
            };

            res.status = httplib::StatusCode::ServiceUnavailable_503;
            res.set_content(response.dump(), "application/json");
            return;
        }

        auto deathsResult = counter.GetDeathCount();
        if (!deathsResult) {
            json response = {
                {"success", false},
                {"error", {
                    {"code", "READ_FAILED"},
                    {"message", "Could not read game data"}
                }}
            };

            res.status = httplib::StatusCode::InternalServerError_500;
            res.set_content(response.dump(), "application/json");
            return;
        }

        json response = {
            {"success", true},
            {"data", {
                {"deaths", *deathsResult}
            }}
        };

        res.set_content(response.dump(), "application/json");
    });
}

int main() {
    auto startTime = std::chrono::steady_clock::now();

    DS3DeathCounter counter;
    httplib::Server server;

    setupRoutes(server, counter, startTime);
    std::cout << "Server running on http://localhost:" << SERVER_PORT << std::endl;
    server.listen("localhost", SERVER_PORT);

    return 0;
}
