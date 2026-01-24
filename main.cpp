#include "discord_rpc.h"
#include "httplib.h"
#include "json.hpp"

#include <tlhelp32.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <ctime>
#include <expected>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

using json = nlohmann::json;

constexpr const char* APP_VERSION = "1.0.0";
constexpr const char* ALLOWED_ORIGIN = "http://localhost:5173";
constexpr int SERVER_PORT = 3000;
constexpr const char* DISCORD_APP_ID = "1464534094405832923";

enum class LogLevel {
    INFO,
    WARN,
    ERR
};

void log(LogLevel level, const std::string& message) {
    std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &now);

    const char* levelStr = "INFO ";
    switch (level) {
        case LogLevel::INFO: levelStr = "INFO "; 
            break;
        case LogLevel::WARN: levelStr = "WARN ";
            break;
        case LogLevel::ERR: levelStr = "ERROR ";
            break;
    }

    std::cout << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S] ") << levelStr << " " << message << std::endl;
}

class DiscordPresence {
private:
    std::atomic<bool> initialized = false;
    int64_t startTimestamp = 0;
    std::string detailsBuffer;
    std::string stateBuffer;

public:
    DiscordPresence() = default;
    DiscordPresence(const DiscordPresence&) = delete;
    DiscordPresence& operator=(const DiscordPresence&) = delete;

    void Initialize() {
        DiscordEventHandlers handlers{};
        handlers.ready = [](const DiscordUser* user) {
            log(LogLevel::INFO, "Discord connected as " + std::string(user->username));
        };
        handlers.disconnected = [](int errorCode, const char* message) {
            log(LogLevel::WARN, "Discord disconnected: " + std::string(message));
        };
        handlers.errored = [](int errorCode, const char* message) {
            log(LogLevel::ERR, "Discord error: " + std::string(message));
        };
        Discord_Initialize(DISCORD_APP_ID, &handlers, 1, nullptr);
        startTimestamp = time(nullptr);
        initialized = true;
        log(LogLevel::INFO, "Discord RPC initialized");
    }

    void Update(uint32_t deaths, uint32_t playtimeMs) {
        if (!initialized) return;

        uint32_t playtimeMinutes = playtimeMs / 1000 / 60;
        uint32_t days = playtimeMinutes / 1440;
        uint32_t hours = (playtimeMinutes % 1440) / 60;
        uint32_t minutes = playtimeMinutes % 60;

        if (deaths == 0) {
            detailsBuffer = "No deaths yet";
        } else {
            detailsBuffer = "Died " + std::to_string(deaths) + (deaths == 1 ? " time" : " times");
        }

        std::ostringstream oss;
        oss << "Current run: ";
        if (days > 0) {
            oss << days << "d ";
        }
        if (hours > 0 || days > 0) {
            oss << hours << "h ";
        }
        oss << minutes << "m";
        stateBuffer = oss.str();

        DiscordRichPresence presence{};
        presence.details = detailsBuffer.c_str();
        presence.state = stateBuffer.c_str();
        presence.largeImageKey = "ds3_logo";
        presence.startTimestamp = startTimestamp;

        Discord_UpdatePresence(&presence);
    }

    void Shutdown() {
        if (initialized) {
            Discord_Shutdown();
            initialized = false;
            log(LogLevel::INFO, "Discord RPC shutdown");
        }
    }

    ~DiscordPresence() {
        Shutdown();
    }
};

DiscordPresence g_discord;
std::atomic<bool> g_running = true;

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

    bool IsInitialized() const {
        return processHandle != nullptr;
    }
};

class DS3StatsReader {
private:
    MemoryReader reader;

    static constexpr uintptr_t GAMEDATAMAN_POINTER = 0x047572B8;
    static constexpr uintptr_t DEATH_COUNT_OFFSET = 0x98;
    static constexpr uintptr_t PLAYTIME_OFFSET = 0xA4;

    static constexpr wchar_t PROCESS_NAME[] = L"DarkSoulsIII.exe";

    std::expected<uint32_t, MemoryReaderError> ReadGameData(uintptr_t basePointer, uintptr_t offset) {
        uintptr_t pointerAddress = reader.GetModuleBase() + basePointer;

        uintptr_t baseAddress = 0;
        if (!reader.ReadMemory(pointerAddress, baseAddress)) {
            return std::unexpected(MemoryReaderError::ReadFailed);
        }

        if (baseAddress == 0) {
            return std::unexpected(MemoryReaderError::ReadFailed);
        }

        uintptr_t dataAddress = baseAddress + offset;

        uint32_t value = 0;
        if (!reader.ReadMemory(dataAddress, value)) {
            return std::unexpected(MemoryReaderError::ReadFailed);
        }

        return value;
    }

public:
    std::expected<void, MemoryReaderError> Initialize() {
        return reader.Initialize(PROCESS_NAME);
    }

    bool IsInitialized() const {
        return reader.IsInitialized();
    }

    std::expected<uint32_t, MemoryReaderError> GetDeathCount() {
        return ReadGameData(GAMEDATAMAN_POINTER, DEATH_COUNT_OFFSET);
    }

    std::expected<uint32_t, MemoryReaderError> GetPlayTime() {
        return ReadGameData(GAMEDATAMAN_POINTER, PLAYTIME_OFFSET);
    }
};

void discordUpdateLoop() {
    DS3StatsReader statsReader;
    bool gameConnected = false;
    uint32_t currentDeaths = 0;
    uint32_t currentPlaytime = 0;
    int minutesSinceSync = 5;

    while (g_running) {
        if (!statsReader.IsInitialized()) {
            if (statsReader.Initialize()) {
                gameConnected = true;
                log(LogLevel::INFO, "Game detected, starting Discord presence");
                minutesSinceSync = 5;
            } else {
                if (gameConnected) {
                    log(LogLevel::WARN, "Game disconnected");
                    Discord_ClearPresence();
                    gameConnected = false;
                }
                Discord_RunCallbacks();
                std::this_thread::sleep_for(std::chrono::minutes(1));
                continue;
            }
        }

        if (minutesSinceSync >= 5) {
            auto deathsResult = statsReader.GetDeathCount();
            auto playTimeResult = statsReader.GetPlayTime();

            if (deathsResult && playTimeResult) {
                currentDeaths = *deathsResult;
                currentPlaytime = *playTimeResult;
            }
            minutesSinceSync = 0;
        }

        g_discord.Update(currentDeaths, currentPlaytime);

        currentPlaytime += 60000;
        minutesSinceSync++;

        Discord_RunCallbacks();
        std::this_thread::sleep_for(std::chrono::minutes(1));
    }
}

void streamStats(DS3StatsReader& statsReader, httplib::DataSink& sink) {
    uint32_t lastDeathCount = 0;
    uint32_t lastPlayTime = 0;
    bool wasConnected = false;
    bool firstRun = true;

    log(LogLevel::INFO, "Client connected to SSE stream");

    while (true) {
        if (!statsReader.IsInitialized()) {
            auto initResult = statsReader.Initialize();
            if (!initResult) {
                if (wasConnected) {
                    log(LogLevel::WARN, "Game disconnected, waiting...");
                    wasConnected = false;
                }

                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }
        }

        if (!wasConnected) {
            log(LogLevel::INFO, "Connected to DarkSoulsIII.exe");
            wasConnected = true;
        }

        auto deathsResult = statsReader.GetDeathCount();
        auto playTimeResult = statsReader.GetPlayTime();

        json data;
        bool changed = false;

        if (deathsResult && (firstRun || *deathsResult != lastDeathCount)) {
            lastDeathCount = *deathsResult;
            data["deaths"] = lastDeathCount;
            changed = true;
        }

        if (playTimeResult && (firstRun || *playTimeResult != lastPlayTime)) {
            lastPlayTime = *playTimeResult;
            data["playtime"] = lastPlayTime;
            changed = true;
        }

        if (changed) {
            std::string event = "data: " + data.dump() + "\n\n";
            if (!sink.write(event.c_str(), event.size())) {
                log(LogLevel::INFO, "Client disconnected from SSE stream");
                return;
            }
            firstRun = false;
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void setupRoutes(httplib::Server& server, DS3StatsReader& statsReader, std::chrono::steady_clock::time_point startTime) {
    server.set_post_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        auto origin = req.get_header_value("Origin");
        if (origin == ALLOWED_ORIGIN) {
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
        if (!statsReader.IsInitialized()) {
            auto initializeResult = statsReader.Initialize();
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
        }

        auto deathsResult = statsReader.GetDeathCount();
        auto playTimeResult = statsReader.GetPlayTime();

        if (!deathsResult || !playTimeResult) {
            if (statsReader.Initialize()) {
                deathsResult = statsReader.GetDeathCount();
                playTimeResult = statsReader.GetPlayTime();
            }
        }

        if (!deathsResult || !playTimeResult) {
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
                {"deaths", *deathsResult},
                {"playtime", *playTimeResult}
            }}
        };

        res.set_content(response.dump(), "application/json");
    });

    server.Get("/api/stats/stream", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Content-Type", "text/event-stream");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");

        res.set_chunked_content_provider("text/event-stream", [](size_t, httplib::DataSink& sink) {
            DS3StatsReader statsReader;
            streamStats(statsReader, sink);
            return false;
        });
    });
}

int main() {
    auto startTime = std::chrono::steady_clock::now();

    log(LogLevel::INFO, "Starting DS3 Stats Reader v" + std::string(APP_VERSION));

    g_discord.Initialize();

    std::thread discordThread(discordUpdateLoop);

    DS3StatsReader statsReader;
    httplib::Server server;

    setupRoutes(server, statsReader, startTime);

    log(LogLevel::INFO, "Server listening on http://localhost:" + std::to_string(SERVER_PORT));
    server.listen("localhost", SERVER_PORT);

    g_running = false;
    discordThread.join();

    g_discord.Shutdown();

    return 0;
}
