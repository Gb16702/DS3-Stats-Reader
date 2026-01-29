#include "core/Log.h"
#include "core/Settings.h"
#include "core/Stats.h"
#include "database/SessionDatabase.h"
#include "discord/DiscordLoop.h"
#include "discord/DiscordPresence.h"
#include "memory/DS3StatsReader.h"
#include "monitoring/GameMonitor.h"
#include "windows/AutoStart.h"
#include "windows/BorderlessWindow.h"
#include "api/Routes.h"

#include "httplib.h"

#include <chrono>
#include <thread>

int main() {
    auto startTime = std::chrono::steady_clock::now();

    log(LogLevel::INFO, "Starting Ember v" + std::string(APP_VERSION));

    g_settings.LoadSettings();
    g_sessionDb.Open();

    if (g_settings.isBorderlessFullscreenEnabled) {
        g_borderlessWindow.Enable();
    }

    if (g_settings.isAutoStartEnabled) {
        AutoStart::Enable();
    }

    std::thread monitorThread(gameMonitorLoop);
    std::thread discordThread(discordUpdateLoop);

    DS3StatsReader statsReader;
    httplib::Server server;

    setupRoutes(server, statsReader, startTime);

    log(LogLevel::INFO, "Starting server on http://localhost:" + std::to_string(SERVER_PORT) + "...");
    server.listen("localhost", SERVER_PORT);

    g_running = false;

    monitorThread.join();
    discordThread.join();

    g_discord.Shutdown();
    g_sessionDb.Close();

    return 0;
}
