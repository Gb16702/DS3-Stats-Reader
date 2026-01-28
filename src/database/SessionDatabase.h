#pragma once

#include "sqlite3.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct Session {
    int id;
    std::string startTime;
    std::string endTime;
    int durationMs;
    int startingDeaths;
    int endingDeaths;
    int sessionDeaths;
    double deathsPerHour;
};

struct PlayerStats {
    int totalDeaths;
    int totalPlaytimeMs;
    std::string lastUpdated;
};

struct Death {
    int id;
    uint32_t zoneId;
    std::string zoneName;
    bool isBossDeath;
    std::string timestamp;
};

class SessionDatabase {
private:
    sqlite3* db = nullptr;
    static constexpr const char* DB_FILE = "sessions.db";

    bool CreateTables();

public:
    SessionDatabase() = default;

    SessionDatabase(const SessionDatabase&) = delete;
    SessionDatabase& operator=(const SessionDatabase&) = delete;

    ~SessionDatabase();

    bool Open();
    bool SaveSession(const std::string& startTime, const std::string& endTime, int durationMs, int startingDeaths, int endingDeaths);
    bool UpdatePlayerStats(int totalDeaths, int totalPlaytimeMs);
    std::optional<PlayerStats> GetPlayerStats();
    std::vector<Session> GetAllSessions();
    bool SaveDeath(uint32_t zoneId, const std::string& zoneName, bool isBossDeath);
    std::vector<Death> GetAllDeaths();
    std::vector<Death> GetBossDeaths();
    std::map<std::string, int> GetDeathCountByBoss();
    std::map<std::string, int> GetDeathCountByZone();
    int GetTotalDeathCount();
    int GetBossDeathCount();
    void Close();
};

extern SessionDatabase g_sessionDb;
