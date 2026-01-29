#include "GameMonitor.h"
#include "../core/Log.h"
#include "../core/Stats.h"
#include "../core/ZoneNames.h"
#include "../database/SessionDatabase.h"
#include "../memory/DS3StatsReader.h"

#include <chrono>
#include <thread>
#include <Windows.h>

std::string g_sessionStartTime;
int g_startingDeaths = -1;
int g_lastKnownDeaths = 0;
int g_lastKnownPlaytime = 0;
int g_currentCharacterId = -1;
std::atomic<bool> g_sessionActive = false;
std::atomic<bool> g_running = true;
CharacterStats g_lastKnownStats{};

static std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

void gameMonitorLoop() {
    DS3StatsReader statsReader;
    bool wasConnected = false;
    auto sessionStartPoint = std::chrono::steady_clock::now();

    bool wasInBossFight = false;
    bool deathRecorded = false;

    while (g_running) {
        if (!statsReader.IsInitialized()) {
            auto initResult = statsReader.Initialize();

            if (initResult) {
                wasConnected = true;
                sessionStartPoint = std::chrono::steady_clock::now();
                log(LogLevel::INFO, "Game detected by monitor");
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
                continue;
            }
        }

        if (!statsReader.IsProcessRunning()) {
            if (wasConnected) {
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
                        g_lastKnownDeaths,
                        g_currentCharacterId
                    );

                    if (g_currentCharacterId > 0) {
                        CharacterStatsRecord statsRecord{};

                        statsRecord.level = g_lastKnownStats.level;
                        statsRecord.vigor = g_lastKnownStats.vigor;
                        statsRecord.attunement = g_lastKnownStats.attunement;
                        statsRecord.endurance = g_lastKnownStats.endurance;
                        statsRecord.vitality = g_lastKnownStats.vitality;
                        statsRecord.strength = g_lastKnownStats.strength;
                        statsRecord.dexterity = g_lastKnownStats.dexterity;
                        statsRecord.intelligence = g_lastKnownStats.intelligence;
                        statsRecord.faith = g_lastKnownStats.faith;
                        statsRecord.luck = g_lastKnownStats.luck;

                        g_sessionDb.SaveCharacterStats(g_currentCharacterId, statsRecord);
                    }

                    g_sessionDb.UpdatePlayerStats(g_lastKnownDeaths, g_lastKnownPlaytime);

                    g_sessionActive = false;
                    g_startingDeaths = -1;
                    g_currentCharacterId = -1;
                }

                wasConnected = false;
            }
            statsReader.Reset();
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            continue;
        }

        auto deathsResult = statsReader.GetDeathCount();
        auto playtimeResult = statsReader.GetPlayTime();

        if (deathsResult && playtimeResult) {
            if (!g_sessionActive && *playtimeResult > 0) {
                g_sessionStartTime = Stats::GetCurrentTimestamp();
                g_startingDeaths = *deathsResult;
                g_lastKnownDeaths = *deathsResult;
                g_lastKnownPlaytime = *playtimeResult;

                auto nameResult = statsReader.GetCharacterName();
                auto classResult = statsReader.GetClass();

                if (nameResult && classResult) {
                    std::string charName = WStringToString(*nameResult);
                    g_currentCharacterId = g_sessionDb.GetOrCreateCharacter(charName, *classResult);
                    log(LogLevel::INFO, "Character: " + charName + " (ID: " + std::to_string(g_currentCharacterId) + ")");
                } else {
                    g_currentCharacterId = -1;
                    log(LogLevel::WARN, "Could not read character info");
                }

                g_sessionActive = true;
                sessionStartPoint = std::chrono::steady_clock::now();
                log(LogLevel::INFO, "Session started with " + std::to_string(g_startingDeaths) + " deaths");
            }
            if (g_sessionActive && *playtimeResult > 0) {
                g_lastKnownDeaths = *deathsResult;
                g_lastKnownPlaytime = *playtimeResult;
                
                if (auto stats = statsReader.GetCharacterStats()) {
					g_lastKnownStats = *stats;
                }
            }

            bool inBossFight = false;
            uint32_t currentZoneId = 0;
            int32_t playerHP = 1;

            auto bossFightResult = statsReader.GetInBossFight();
            if (bossFightResult) {
                inBossFight = *bossFightResult;
            }

            auto zoneResult = statsReader.GetPlayRegion();
            if (zoneResult) {
                currentZoneId = *zoneResult;
            }

            auto hpResult = statsReader.GetPlayerHP();
            if (hpResult) {
                playerHP = *hpResult;
            }

            if (inBossFight && !wasInBossFight) {
                log(LogLevel::INFO, "Entered boss fight: " + GetZoneName(currentZoneId));
            }

            if (playerHP <= 0 && !deathRecorded && currentZoneId != 0 && g_currentCharacterId > 0) {
                std::string zoneName = GetZoneName(currentZoneId);
                g_sessionDb.SaveDeath(currentZoneId, zoneName, g_currentCharacterId, inBossFight);
                deathRecorded = true;
            }

            if (playerHP > 0 && deathRecorded) {
                deathRecorded = false;
            }

            wasInBossFight = inBossFight;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
}
