#include "SessionDatabase.h"
#include "../core/Log.h"
#include "../core/Stats.h"

SessionDatabase g_sessionDb;

bool SessionDatabase::CreateTables() {
    const char* charactersSql = R"(
        CREATE TABLE IF NOT EXISTS characters (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            class_id INTEGER NOT NULL,
            created_at TEXT,
            UNIQUE(name, class_id)
        )
    )";

    const char* sessionsSql = R"(
        CREATE TABLE IF NOT EXISTS sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            start_time TEXT,
            end_time TEXT,
            duration_ms INTEGER,
            starting_deaths INTEGER,
            ending_deaths INTEGER,
            session_deaths INTEGER,
            deaths_per_hour REAL,
            character_id INTEGER,
            FOREIGN KEY (character_id) REFERENCES characters(id)
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
            character_id INTEGER,
            timestamp TEXT,
            is_boss_death INTEGER DEFAULT 0,
            FOREIGN KEY (character_id) REFERENCES characters(id)
        )
    )";

    const char* characterStatsSql = R"(
        CREATE TABLE IF NOT EXISTS character_stats (
            character_id INTEGER PRIMARY KEY,
            level INTEGER,
            vigor INTEGER,
            attunement INTEGER,
            endurance INTEGER,
            vitality INTEGER,
            strength INTEGER,
            dexterity INTEGER,
            intelligence INTEGER,
            faith INTEGER,
            luck INTEGER,
            updated_at TEXT,
            FOREIGN KEY (character_id) REFERENCES characters(id)
      )
    )";

    char* errMsg = nullptr;

    if (sqlite3_exec(db, charactersSql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to create characters table: " + std::string(errMsg));
        sqlite3_free(errMsg);
        return false;
    }

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
    if (sqlite3_exec(db, characterStatsSql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to create character_stats table: " + std::string(errMsg));
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

bool SessionDatabase::SaveSession(const std::string& startTime, const std::string& endTime, int durationMs, int startingDeaths, int endingDeaths, int characterId) {
    int sessionDeaths = endingDeaths - startingDeaths;
    double deathsPerHour = Stats::CalculateDeathsPerHour(sessionDeaths, durationMs);

    const char* sql = R"(
        INSERT INTO sessions(start_time, end_time, duration_ms, starting_deaths, ending_deaths, session_deaths, deaths_per_hour, character_id)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
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
    sqlite3_bind_int(stmt, 8, characterId);

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
        SELECT id, start_time, end_time, duration_ms, starting_deaths, ending_deaths, session_deaths, deaths_per_hour, character_id
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

        if (auto text = sqlite3_column_text(stmt, 1)) {
            session.startTime = reinterpret_cast<const char*>(text);
        }

        if (auto text = sqlite3_column_text(stmt, 2)) {
            session.endTime = reinterpret_cast<const char*>(text);
        }

        session.durationMs = sqlite3_column_int(stmt, 3);
        session.startingDeaths = sqlite3_column_int(stmt, 4);
        session.endingDeaths = sqlite3_column_int(stmt, 5);
        session.sessionDeaths = sqlite3_column_int(stmt, 6);
        session.deathsPerHour = sqlite3_column_double(stmt, 7);
        session.characterId = sqlite3_column_int(stmt, 8);

        sessions.push_back(session);
    }

    sqlite3_finalize(stmt);
    return sessions;
}

bool SessionDatabase::SaveDeath(uint32_t zoneId, const std::string& zoneName, int characterId, bool isBossDeath) {
    const char* sql = R"(
        INSERT INTO deaths(zone_id, zone_name, character_id, timestamp, is_boss_death)
        VALUES (?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare SaveDeath");
        return false;
    }

    std::string timestamp = Stats::GetCurrentTimestamp();

    sqlite3_bind_int(stmt, 1, static_cast<int>(zoneId));
    sqlite3_bind_text(stmt, 2, zoneName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, characterId);
    sqlite3_bind_text(stmt, 4, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, isBossDeath ? 1 : 0);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        log(LogLevel::ERR, "Failed to save death");
        return false;
    }

    log(LogLevel::INFO, "Death saved: " + zoneName + (isBossDeath ? " (boss)" : ""));
    return true;
}

std::vector<Death> SessionDatabase::GetAllDeaths() {
    std::vector<Death> deaths;

    const char* sql = R"(
        SELECT id, zone_id, zone_name, character_id, timestamp, is_boss_death
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

        death.characterId = sqlite3_column_int(stmt, 3);

        const unsigned char* timestampText = sqlite3_column_text(stmt, 4);
        if (timestampText) {
            death.timestamp = reinterpret_cast<const char*>(timestampText);
        }

        death.isBossDeath = sqlite3_column_int(stmt, 5) != 0;

        deaths.push_back(death);
    }

    sqlite3_finalize(stmt);
    return deaths;
}

std::vector<Death> SessionDatabase::GetDeathsByCharacter(int characterId) {
    std::vector<Death> deaths;

    const char* sql = R"(
        SELECT id, zone_id, zone_name, character_id, timestamp, is_boss_death
        FROM deaths
        WHERE character_id = ?
        ORDER BY id DESC
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare GetDeathsByCharacter");
        return deaths;
    }

    sqlite3_bind_int(stmt, 1, characterId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Death death;
        death.id = sqlite3_column_int(stmt, 0);
        death.zoneId = static_cast<uint32_t>(sqlite3_column_int(stmt, 1));

        const unsigned char* nameText = sqlite3_column_text(stmt, 2);
        if (nameText) {
            death.zoneName = reinterpret_cast<const char*>(nameText);
        }

        death.characterId = sqlite3_column_int(stmt, 3);

        const unsigned char* timestampText = sqlite3_column_text(stmt, 4);
        if (timestampText) {
            death.timestamp = reinterpret_cast<const char*>(timestampText);
        }

        death.isBossDeath = sqlite3_column_int(stmt, 5) != 0;

        deaths.push_back(death);
    }

    sqlite3_finalize(stmt);
    return deaths;
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

std::map<std::string, int> SessionDatabase::GetDeathCountByZoneForCharacter(int characterId) {
    std::map<std::string, int> result;

    const char* sql = R"(
        SELECT zone_name, COUNT(*) as death_count
        FROM deaths
        WHERE character_id = ?
        GROUP BY zone_id
        ORDER BY death_count DESC
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare GetDeathCountByZoneForCharacter");
        return result;
    }

    sqlite3_bind_int(stmt, 1, characterId);

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

int SessionDatabase::GetDeathCountForCharacter(int characterId) {
    const char* sql = "SELECT COUNT(*) FROM deaths WHERE character_id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare GetDeathCountForCharacter");
        return 0;
    }

    sqlite3_bind_int(stmt, 1, characterId);

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

int SessionDatabase::GetOrCreateCharacter(const std::string& name, int classId) {
    const char* selectSql = "SELECT id FROM characters WHERE name = ? AND class_id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, selectSql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare GetOrCreateCharacter select");
        return -1;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, classId);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return id;
    }
    sqlite3_finalize(stmt);

    const char* insertSql = R"(
        INSERT INTO characters(name, class_id, created_at)
        VALUES (?, ?, ?)
    )";

    if (sqlite3_prepare_v2(db, insertSql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare GetOrCreateCharacter insert");
        return -1;
    }

    std::string timestamp = Stats::GetCurrentTimestamp();

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, classId);
    sqlite3_bind_text(stmt, 3, timestamp.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        log(LogLevel::ERR, "Failed to insert character");
        sqlite3_finalize(stmt);
        return -1;
    }

    int newId = static_cast<int>(sqlite3_last_insert_rowid(db));
    sqlite3_finalize(stmt);

    log(LogLevel::INFO, "New character created: " + name);
    return newId;
}

std::optional<Character> SessionDatabase::GetCharacter(int id) {
    const char* sql = "SELECT id, name, class_id, created_at FROM characters WHERE id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare GetCharacter");
        return std::nullopt;
    }

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    Character character;
    character.id = sqlite3_column_int(stmt, 0);

    const unsigned char* nameText = sqlite3_column_text(stmt, 1);
    if (nameText) {
        character.name = reinterpret_cast<const char*>(nameText);
    }

    character.classId = sqlite3_column_int(stmt, 2);

    const unsigned char* createdText = sqlite3_column_text(stmt, 3);
    if (createdText) {
        character.createdAt = reinterpret_cast<const char*>(createdText);
    }

    sqlite3_finalize(stmt);
    return character;
}

std::vector<Character> SessionDatabase::GetAllCharacters() {
    std::vector<Character> characters;

    const char* sql = "SELECT id, name, class_id, created_at FROM characters ORDER BY id";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare GetAllCharacters");
        return characters;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Character character;
        character.id = sqlite3_column_int(stmt, 0);

        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        if (nameText) {
            character.name = reinterpret_cast<const char*>(nameText);
        }

        character.classId = sqlite3_column_int(stmt, 2);

        const unsigned char* createdText = sqlite3_column_text(stmt, 3);
        if (createdText) {
            character.createdAt = reinterpret_cast<const char*>(createdText);
        }

        characters.push_back(character);
    }

    sqlite3_finalize(stmt);
    return characters;
}

bool SessionDatabase::SaveCharacterStats(int characterId, const CharacterStatsRecord& statsRecord) {
    const char* sql = R"(
        INSERT OR REPLACE INTO character_stats(
            character_id, level, vigor, attunement, endurance, vitality,
            strength, dexterity, intelligence, faith, luck, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare SaveCharacterStats");
        return false;
    }

    std::string timestamp = Stats::GetCurrentTimestamp();

    sqlite3_bind_int(stmt, 1, characterId);
    sqlite3_bind_int(stmt, 2, statsRecord.level);
    sqlite3_bind_int(stmt, 3, statsRecord.vigor);
    sqlite3_bind_int(stmt, 4, statsRecord.attunement);
    sqlite3_bind_int(stmt, 5, statsRecord.endurance);
    sqlite3_bind_int(stmt, 6, statsRecord.vitality);
    sqlite3_bind_int(stmt, 7, statsRecord.strength);
    sqlite3_bind_int(stmt, 8, statsRecord.dexterity);
    sqlite3_bind_int(stmt, 9, statsRecord.intelligence);
    sqlite3_bind_int(stmt, 10, statsRecord.faith);
    sqlite3_bind_int(stmt, 11, statsRecord.luck);
    sqlite3_bind_text(stmt, 12, timestamp.c_str(), -1, SQLITE_TRANSIENT);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

std::optional<CharacterStatsRecord> SessionDatabase::GetCharacterStats(int characterId) {
    const char* sql = R"(
        SELECT character_id, level, vigor, attunement, endurance, vitality,
               strength, dexterity, intelligence, faith, luck, updated_at
        FROM character_stats WHERE character_id = ?
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log(LogLevel::ERR, "Failed to prepare GetCharacterStats");
        return std::nullopt;
    }

    sqlite3_bind_int(stmt, 1, characterId);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    CharacterStatsRecord statsRecord;
    statsRecord.characterId = sqlite3_column_int(stmt, 0);
    statsRecord.level = sqlite3_column_int(stmt, 1);
    statsRecord.vigor = sqlite3_column_int(stmt, 2);
    statsRecord.attunement = sqlite3_column_int(stmt, 3);
    statsRecord.endurance = sqlite3_column_int(stmt, 4);
    statsRecord.vitality = sqlite3_column_int(stmt, 5);
    statsRecord.strength = sqlite3_column_int(stmt, 6);
    statsRecord.dexterity = sqlite3_column_int(stmt, 7);
    statsRecord.intelligence = sqlite3_column_int(stmt, 8);
    statsRecord.faith = sqlite3_column_int(stmt, 9);
    statsRecord.luck = sqlite3_column_int(stmt, 10);

    if (auto text = sqlite3_column_text(stmt, 11)) {
        statsRecord.updatedAt = reinterpret_cast<const char*>(text);
    }

    sqlite3_finalize(stmt);
    return statsRecord;
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
