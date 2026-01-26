#include "GameMonitor.h"
#include "../core/Log.h"
#include "../core/Stats.h"
#include "../database/SessionDatabase.h"
#include "../memory/DS3StatsReader.h"

#include <chrono>
#include <thread>

std::string g_sessionStartTime;
int g_startingDeaths = -1;
int g_lastKnownDeaths = 0;
int g_lastKnownPlaytime = 0;
std::atomic<bool> g_sessionActive = false;
std::atomic<bool> g_running = true;

void gameMonitorLoop() {
    DS3StatsReader statsReader;
    bool wasConnected = false;
    auto sessionStartPoint = std::chrono::steady_clock::now();

    while (g_running) {
        if (!statsReader.IsInitialized()) {
            auto initResult = statsReader.Initialize();

            if (initResult) {
                wasConnected = true;
                sessionStartPoint = std::chrono::steady_clock::now();
                log(LogLevel::INFO, "Game detected by monitor");
            } else {
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }
        }

        auto deathsResult = statsReader.GetDeathCount();
        auto playtimeResult = statsReader.GetPlayTime();

        if (deathsResult && playtimeResult) {
            if (!g_sessionActive && *playtimeResult > 0) {
                g_sessionStartTime = Stats::GetCurrentTimestamp();
                g_startingDeaths = *deathsResult;
                g_lastKnownDeaths = *deathsResult;
                g_lastKnownPlaytime = *playtimeResult;
                g_sessionActive = true;
                sessionStartPoint = std::chrono::steady_clock::now();
                log(LogLevel::INFO, "Session started with " + std::to_string(g_startingDeaths) + " deaths");
            }
            if (g_sessionActive && *playtimeResult > 0) {
                g_lastKnownDeaths = *deathsResult;
                g_lastKnownPlaytime = *playtimeResult;
            }
        } else {
            auto reinitResult = statsReader.Initialize();

            if (!reinitResult && wasConnected) {
                log(LogLevel::INFO, "Game closed");

                if (g_sessionActive) {
                    auto endPoint = std::chrono::steady_clock::now();
                    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endPoint - sessionStartPoint).count();
                    std::string endTimestamp = Stats::GetCurrentTimestamp();

                    g_sessionDb.SaveSession(
                        g_sessionStartTime,
                        endTimestamp,
                        static_cast<int>(durationMs),
                        g_startingDeaths,
                        g_lastKnownDeaths
                    );

                    g_sessionDb.UpdatePlayerStats(g_lastKnownDeaths, g_lastKnownPlaytime);

                    g_sessionActive = false;
                    g_startingDeaths = -1;
                }

                wasConnected = false;
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}
