#pragma once

#include "sqlite3.h"

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
    void Close();
};

extern SessionDatabase g_sessionDb;
