#include "discord_rpc.h"
#include "httplib.h"
#include "json.hpp"

#include <tlhelp32.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <expected>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <syncstream>
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
        case LogLevel::INFO:
            levelStr = "INFO "; 
            break;

        case LogLevel::WARN:
            levelStr = "WARN ";
            break;

        case LogLevel::ERR:
            levelStr = "ERROR ";
            break;
    }

    std::osyncstream(std::cout) << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S] ") << levelStr << message << std::endl;
}

struct Settings {
    static constexpr const char* FILENAME = "settings.json";

    std::atomic<bool> isDeathCountVisible = true;
    std::atomic<bool> isPlaytimeVisible = true;
    std::atomic<bool> isDiscordRpcEnabled = true;
    std::atomic<bool> isBorderlessFullscreenEnabled = false;
    std::atomic<bool> isAutoStartEnabled = false;

    void LoadSettings() {
        std::ifstream settingsFile(FILENAME);
        if (!settingsFile) {
            log(LogLevel::INFO, "No settings file, creating defaults");
            SaveSettings();
            return;
        }

        try {
            json settingsData;
            settingsFile >> settingsData;

            isDeathCountVisible = settingsData.value("isDeathCountVisible", true);
            isPlaytimeVisible = settingsData.value("isPlaytimeVisible", true);
            isDiscordRpcEnabled = settingsData.value("isDiscordRpcEnabled", true);
            isBorderlessFullscreenEnabled = settingsData.value("isBorderlessFullscreenEnabled", false);
            isAutoStartEnabled = settingsData.value("isAutoStartEnabled", false);
        }
        catch (...) {
            log(LogLevel::WARN, "Invalid settings.json, restoring defaults");
            SaveSettings();
        }
    }
    
    void SaveSettings() {
        json settingsData = {
            {"isDeathCountVisible", isDeathCountVisible.load()},
            {"isPlaytimeVisible", isPlaytimeVisible.load()},
            {"isDiscordRpcEnabled", isDiscordRpcEnabled.load()},
            {"isBorderlessFullscreenEnabled", isBorderlessFullscreenEnabled.load()},
            {"isAutoStartEnabled", isAutoStartEnabled.load()},
        };

        std::ofstream settingsFile(FILENAME);
        if (!settingsFile) {
            log(LogLevel::ERR, "Failed to save settings");
            return;
        }

        settingsFile << settingsData.dump(4);
    }
};

Settings g_settings;

class BorderlessWindow {
private:
    static constexpr wchar_t WINDOW_TITLE[] = L"DARK SOULS III";

    HWND windowHandle = nullptr;
    LONG windowedStyle = 0;
    RECT windowedRect = {};
    bool isActive = false;

    bool FindGameWindow() {
        windowHandle = ::FindWindowW(nullptr, WINDOW_TITLE);
        
        if (windowHandle == nullptr) {
            log(LogLevel::WARN, "Could not find Dark Souls III window");
            return false;
        }

        return true;
    }

public:
    BorderlessWindow() = default;

    BorderlessWindow(const BorderlessWindow&) = delete;
    BorderlessWindow& operator=(const BorderlessWindow&) = delete;

    bool Enable() {
        if (isActive) {
            log(LogLevel::WARN, "Borderless mode already enabled");
            return true;
        }

        if (!FindGameWindow()) {
            return false;
        }

        windowedStyle = ::GetWindowLongW(windowHandle, GWL_STYLE);
        if (windowedStyle == 0) {
            log(LogLevel::ERR, "Failed to get window style");
            return false;
        }

        if (!::GetWindowRect(windowHandle, &windowedRect)) {
            log(LogLevel::ERR, "Failed to get window rect");
            return false;
        }

        LONG borderlessWindowStyle = windowedStyle & ~(WS_CAPTION | WS_THICKFRAME | WS_BORDER);
        ::SetWindowLongW(windowHandle, GWL_STYLE, borderlessWindowStyle);

        int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);

        ::SetWindowPos(windowHandle, HWND_TOP, 0, 0, screenWidth, screenHeight, SWP_FRAMECHANGED);

        isActive = true;
        log(LogLevel::INFO, "Borderless fullscreen enabled");

        return true;
    }

    bool Disable() {
        if (!isActive) {
            log(LogLevel::WARN, "Borderless mode already disabled");
            return true;
        }

        if (!::IsWindow(windowHandle)) {
            log(LogLevel::ERR, "Window no longer exists");
            isActive = false;
            return false;
        }

        ::SetWindowLongW(windowHandle, GWL_STYLE, windowedStyle);
        ::SetWindowPos(windowHandle, HWND_NOTOPMOST, windowedRect.left, windowedRect.top, windowedRect.right - windowedRect.left, windowedRect.bottom - windowedRect.top, SWP_FRAMECHANGED);

        isActive = false;
        log(LogLevel::INFO, "Borderless fullscreen disabled");

        return true;
    }

    bool IsActive() const {
        
        return isActive;
    }
};

BorderlessWindow g_borderlessWindow;

class AutoStart {
private:
    static constexpr const wchar_t* RUN_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    static constexpr const wchar_t* APP_NAME = L"DS3StatsReader";

public:
    static bool Enable() {
        wchar_t exePath[MAX_PATH];
        if (::GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) {
            log(LogLevel::ERR, "Failed to get executable path");
            return false;
        }

        HKEY hkey;
        LONG result = ::RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &hkey);

        if (result != ERROR_SUCCESS) {
            log(LogLevel::ERR, "Failed to open registry key");
            return false;
        }

        result = ::RegSetValueExW(hkey, APP_NAME, 0, REG_SZ, reinterpret_cast<const BYTE*>(exePath), static_cast<DWORD>((wcslen(exePath) + 1) * sizeof(wchar_t)));
        ::RegCloseKey(hkey);

        if (result != ERROR_SUCCESS) {
            log(LogLevel::ERR, "Failed to set registry value");
            return false;
        }

        log(LogLevel::INFO, "Auto-start enabled");
        
        return true;
    }

    static bool Disable() {
        HKEY hkey;
        LONG result = ::RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &hkey);

        if (result != ERROR_SUCCESS) {
            log(LogLevel::ERR, "Failed to open registry key");
            return false;
        }

        result = ::RegDeleteValueW(hkey, APP_NAME);
        ::RegCloseKey(hkey);

        if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
            log(LogLevel::ERR, "Failed to delete registry value");
            return false;
        }

        log(LogLevel::INFO, "Auto-start disabled");
        return true;
    }
};

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
        Discord_RunCallbacks();
        
        startTimestamp = time(nullptr);
        initialized = true;

        log(LogLevel::INFO, "Discord remote procedure call initialized");
    }

    void Update(uint32_t deaths, uint32_t playtimeMs) {
        if (!initialized) {
            return;
        }

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
            log(LogLevel::INFO, "Discord remote procedure call shutdown");
        }
    }

    ~DiscordPresence() {
        Shutdown();
    }
};

DiscordPresence g_discord;
std::atomic<bool> g_running = true;
std::mutex g_discordMutex;
std::condition_variable g_discordCv;

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

    g_discord.Initialize();

    while (g_running) {
        std::unique_lock<std::mutex> lock(g_discordMutex);

        if (!g_settings.isDiscordRpcEnabled) {
            Discord_ClearPresence();
            g_discordCv.wait_for(lock, std::chrono::seconds(15));
            continue;
        }

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
                g_discordCv.wait_for(lock, std::chrono::seconds(15));
                continue;
            }
        }

        if (minutesSinceSync >= 5) {
            auto deathsResult = statsReader.GetDeathCount();

            if (deathsResult) {
                currentDeaths = *deathsResult;
            }
            minutesSinceSync = 0;
        }

        auto playtimeResult = statsReader.GetPlayTime();
        if (playtimeResult) {
            currentPlaytime = *playtimeResult;
        }

        g_discord.Update(currentDeaths, currentPlaytime);

        minutesSinceSync++;

        Discord_RunCallbacks();
        g_discordCv.wait_for(lock, std::chrono::seconds(15));
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

        if (g_settings.isDeathCountVisible && deathsResult && (firstRun || *deathsResult != lastDeathCount)) {
            lastDeathCount = *deathsResult;
            data["deaths"] = lastDeathCount;
            changed = true;
        }

        if (g_settings.isPlaytimeVisible && playTimeResult && (firstRun || *playTimeResult != lastPlayTime)) {
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

    server.Get("/api/settings", [](const httplib::Request& req, httplib::Response& res) {
        json response = {
            {"success", true},
            {"data", {
                {"isDeathCountVisible", g_settings.isDeathCountVisible.load()},
                {"isPlaytimeVisible", g_settings.isPlaytimeVisible.load()},
                {"isDiscordRpcEnabled", g_settings.isDiscordRpcEnabled.load()},
                {"isBorderlessFullscreenEnabled", g_settings.isBorderlessFullscreenEnabled.load()},
                {"isAutoStartEnabled", g_settings.isAutoStartEnabled.load()},
            }}
        };

        res.set_content(response.dump(), "application/json");
    });

    server.Patch("/api/settings", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);

            if (body.contains("isDeathCountVisible")) {
                g_settings.isDeathCountVisible = body["isDeathCountVisible"];
            }

            if (body.contains("isPlaytimeVisible")) {
                g_settings.isPlaytimeVisible = body["isPlaytimeVisible"];
            }

            if (body.contains("isDiscordRpcEnabled")) {
                g_settings.isDiscordRpcEnabled = body["isDiscordRpcEnabled"];
            }

            if (body.contains("isBorderlessFullscreenEnabled")) {
                bool enabled = body["isBorderlessFullscreenEnabled"];
                g_settings.isBorderlessFullscreenEnabled = enabled;

                enabled ? g_borderlessWindow.Enable() : g_borderlessWindow.Disable();
            }

            if (body.contains("isAutoStartEnabled")) {
                bool enabled = body["isAutoStartEnabled"];
                g_settings.isAutoStartEnabled = enabled;

                enabled ? AutoStart::Enable() : AutoStart::Disable();
            }

            g_settings.SaveSettings();
            g_discordCv.notify_one();

            json response = {
                {"success", true},
                {"data", {
                    {"isDeathCountVisible", g_settings.isDeathCountVisible.load()},
                    {"isPlaytimeVisible", g_settings.isPlaytimeVisible.load()},
                    {"isDiscordRpcEnabled", g_settings.isDiscordRpcEnabled.load()},
                    {"isBorderlessFullscreenEnabled", g_settings.isBorderlessFullscreenEnabled.load()},
                    {"isAutoStartEnabled", g_settings.isAutoStartEnabled.load()}
                }}
            };

            res.set_content(response.dump(), "application/json");
        } catch (...) {
            json response = {
                {"success", false},
                {"error", "Invalid request body"}
            };

            res.status = httplib::StatusCode::BadRequest_400;
            res.set_content(response.dump(), "application/json");
        }
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

    g_settings.LoadSettings();
    if (g_settings.isBorderlessFullscreenEnabled) {
		g_borderlessWindow.Enable();
    }

    if (g_settings.isAutoStartEnabled) {
        AutoStart::Enable();
    }


    std::thread discordThread(discordUpdateLoop);

    DS3StatsReader statsReader;
    httplib::Server server;

    setupRoutes(server, statsReader, startTime);

    log(LogLevel::INFO, "Starting server on http://localhost:" + std::to_string(SERVER_PORT) + "...");
	server.listen("localhost", SERVER_PORT);

    g_running = false;

    discordThread.join();

    g_discord.Shutdown();

    return 0;
}
