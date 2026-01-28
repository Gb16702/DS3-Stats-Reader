#include "SessionDatabase.h"
#include "../core/Log.h"
#include "../core/Stats.h"

SessionDatabase g_sessionDb;

bool SessionDatabase::CreateTables() {
    const char* sessionsSql = R"(
        CREATE TABLE IF NOT EXISTS sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            start_time TEXT,
            end_time TEXT,
            duration_ms INTEGER,
            starting_deaths INTEGER,
            ending_deaths INTEGER,
            session_deaths INTEGER,
            deaths_per_hour REAL
        )
    )";

    const char* playerStatsSql = R"(
        CREATE TABLE IF NOT EXISTS player_stats (
            id INTEGER PRIMARY KEY,
            total_deaths INTEGER,
            total_playtime_ms INTEGER,
            last_updated TEXT
        )
    )";

    const char* deathsSql = R"(
        CREATE TABLE IF NOT EXISTS deaths (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            zone_id INTEGER,
            zone_name TEXT,
            is_boss_death INTEGER,
            timestamp TEXT
        )
    )";

    char* errMsg = nullptr;

    if (sqlite3_exec(db, sessionsSql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to create sessions table: " + std::string(errMsg));
        sqlite3_free(errMsg);
        return false;
    }

    if (sqlite3_exec(db, playerStatsSql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to create player_stats table: " + std::string(errMsg));
        sqlite3_free(errMsg);
        return false;
    }

    if (sqlite3_exec(db, deathsSql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to create deaths table: " + std::string(errMsg));
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

bool SessionDatabase::Open() {
    int result = sqlite3_open(DB_FILE, &db);
    if (result != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to open database: " + std::string(sqlite3_errmsg(db)));
        return false;
    }

    if (!CreateTables()) {
        return false;
    }

    log(LogLevel::INFO, "Database opened");
    return true;
}

bool SessionDatabase::SaveSession(const std::string& startTime, const std::string& endTime, int durationMs, int startingDeaths, int endingDeaths) {
    int sessionDeaths = endingDeaths - startingDeaths;
    double deathsPerHour = Stats::CalculateDeathsPerHour(sessionDeaths, durationMs);

    const char* sql = R"(
        INSERT INTO sessions(start_time, end_time, duration_ms, starting_deaths, ending_deaths, session_deaths, deaths_per_hour)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare statement");
        return false;
    }

    sqlite3_bind_text(stmt, 1, startTime.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, endTime.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, durationMs);
    sqlite3_bind_int(stmt, 4, startingDeaths);
    sqlite3_bind_int(stmt, 5, endingDeaths);
    sqlite3_bind_int(stmt, 6, sessionDeaths);
    sqlite3_bind_double(stmt, 7, deathsPerHour);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        log(LogLevel::ERR, "Failed to save session");
        return false;
    }

    log(LogLevel::INFO, "Session saved: " + std::to_string(sessionDeaths) + " deaths");
    return true;
}

bool SessionDatabase::UpdatePlayerStats(int totalDeaths, int totalPlaytimeMs) {
    const char* sql = R"(
        INSERT OR REPLACE INTO player_stats(id, total_deaths, total_playtime_ms, last_updated)
        values (1, ?, ?, ?)    
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare UpdatePlayerStats");
        return false;
    }

    std::string timestamp = Stats::GetCurrentTimestamp();

    sqlite3_bind_int(stmt, 1, totalDeaths);
    sqlite3_bind_int(stmt, 2, totalPlaytimeMs);
    sqlite3_bind_text(stmt, 3, timestamp.c_str(), -1, SQLITE_TRANSIENT);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

std::optional<PlayerStats> SessionDatabase::GetPlayerStats() {
    const char* sql = "SELECT total_deaths, total_playtime_ms, last_updated FROM player_stats WHERE id = 1";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare GetPlayerStats");
        return std::nullopt;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    PlayerStats stats;
    stats.totalDeaths = sqlite3_column_int(stmt, 0);
    stats.totalPlaytimeMs = sqlite3_column_int(stmt, 1);

    const unsigned char* lastUpdatedText = sqlite3_column_text(stmt, 2);
    if (lastUpdatedText) {
        stats.lastUpdated = reinterpret_cast<const char*>(lastUpdatedText);
    }

    sqlite3_finalize(stmt);
    return stats;
}

std::vector<Session> SessionDatabase::GetAllSessions() {
    std::vector<Session> sessions;

    const char* sql = R"(
          SELECT id, start_time, end_time, duration_ms, starting_deaths, ending_deaths, session_deaths, deaths_per_hour
          FROM sessions
          ORDER BY id DESC
      )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare GetAllSessions");
        return sessions;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Session session;
        session.id = sqlite3_column_int(stmt, 0);

        const unsigned char* startText = sqlite3_column_text(stmt, 1);
        if (startText) {
            session.startTime = reinterpret_cast<const char*>(startText);
        }

        const unsigned char* endText = sqlite3_column_text(stmt, 2);
        if (endText) {
            session.endTime = reinterpret_cast<const char*>(endText);
        }

        session.durationMs = sqlite3_column_int(stmt, 3);
        session.startingDeaths = sqlite3_column_int(stmt, 4);
        session.endingDeaths = sqlite3_column_int(stmt, 5);
        session.sessionDeaths = sqlite3_column_int(stmt, 6);
        session.deathsPerHour = sqlite3_column_double(stmt, 7);

        sessions.push_back(session);
    }

    sqlite3_finalize(stmt);
    return sessions;
}

bool SessionDatabase::SaveDeath(uint32_t zoneId, const std::string& zoneName, bool isBossDeath) {
    const char* sql = R"(
        INSERT INTO deaths(zone_id, zone_name, is_boss_death, timestamp)
        VALUES (?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare SaveDeath");
        return false;
    }

    std::string timestamp = Stats::GetCurrentTimestamp();

    sqlite3_bind_int(stmt, 1, static_cast<int>(zoneId));
    sqlite3_bind_text(stmt, 2, zoneName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, isBossDeath ? 1 : 0);
    sqlite3_bind_text(stmt, 4, timestamp.c_str(), -1, SQLITE_TRANSIENT);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        log(LogLevel::ERR, "Failed to save death");
        return false;
    }

    std::string deathType = isBossDeath ? "Boss death" : "Death";
    log(LogLevel::INFO, deathType + " saved: " + zoneName);
    return true;
}

std::vector<Death> SessionDatabase::GetAllDeaths() {
    std::vector<Death> deaths;

    const char* sql = R"(
        SELECT id, zone_id, zone_name, is_boss_death, timestamp
        FROM deaths
        ORDER BY id DESC
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare GetAllDeaths");
        return deaths;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Death death;
        death.id = sqlite3_column_int(stmt, 0);
        death.zoneId = static_cast<uint32_t>(sqlite3_column_int(stmt, 1));

        const unsigned char* nameText = sqlite3_column_text(stmt, 2);
        if (nameText) {
            death.zoneName = reinterpret_cast<const char*>(nameText);
        }

        death.isBossDeath = sqlite3_column_int(stmt, 3) != 0;

        const unsigned char* timestampText = sqlite3_column_text(stmt, 4);
        if (timestampText) {
            death.timestamp = reinterpret_cast<const char*>(timestampText);
        }

        deaths.push_back(death);
    }

    sqlite3_finalize(stmt);
    return deaths;
}

std::vector<Death> SessionDatabase::GetBossDeaths() {
    std::vector<Death> deaths;

    const char* sql = R"(
        SELECT id, zone_id, zone_name, is_boss_death, timestamp
        FROM deaths
        WHERE is_boss_death = 1
        ORDER BY id DESC
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare GetBossDeaths");
        return deaths;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Death death;
        death.id = sqlite3_column_int(stmt, 0);
        death.zoneId = static_cast<uint32_t>(sqlite3_column_int(stmt, 1));

        const unsigned char* nameText = sqlite3_column_text(stmt, 2);
        if (nameText) {
            death.zoneName = reinterpret_cast<const char*>(nameText);
        }

        death.isBossDeath = true;

        const unsigned char* timestampText = sqlite3_column_text(stmt, 4);
        if (timestampText) {
            death.timestamp = reinterpret_cast<const char*>(timestampText);
        }

        deaths.push_back(death);
    }

    sqlite3_finalize(stmt);
    return deaths;
}

std::map<std::string, int> SessionDatabase::GetDeathCountByBoss() {
    std::map<std::string, int> result;

    const char* sql = R"(
        SELECT zone_name, COUNT(*) as death_count
        FROM deaths
        WHERE is_boss_death = 1
        GROUP BY zone_id
        ORDER BY death_count DESC
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare GetDeathCountByBoss");
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* nameText = sqlite3_column_text(stmt, 0);
        int count = sqlite3_column_int(stmt, 1);

        if (nameText) {
            result[reinterpret_cast<const char*>(nameText)] = count;
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

std::map<std::string, int> SessionDatabase::GetDeathCountByZone() {
    std::map<std::string, int> result;

    const char* sql = R"(
        SELECT zone_name, COUNT(*) as death_count
        FROM deaths
        GROUP BY zone_id
        ORDER BY death_count DESC
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare GetDeathCountByZone");
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* nameText = sqlite3_column_text(stmt, 0);
        int count = sqlite3_column_int(stmt, 1);

        if (nameText) {
            result[reinterpret_cast<const char*>(nameText)] = count;
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

int SessionDatabase::GetTotalDeathCount() {
    const char* sql = "SELECT COUNT(*) FROM deaths";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare GetTotalDeathCount");
        return 0;
    }

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

int SessionDatabase::GetBossDeathCount() {
    const char* sql = "SELECT COUNT(*) FROM deaths WHERE is_boss_death = 1";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare GetBossDeathCount");
        return 0;
    }

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

void SessionDatabase::Close() {
    if (db) {
        sqlite3_close_v2(db);
        db = nullptr;
        log(LogLevel::INFO, "Session database closed");
    }
}

SessionDatabase::~SessionDatabase() {
    Close();
}
