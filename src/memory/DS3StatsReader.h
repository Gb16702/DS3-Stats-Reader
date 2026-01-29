#pragma once

#include "MemoryReader.h"

#include <expected>
#include <cstdint>
#include <string>

struct CharacterStats {
    uint32_t level;
    uint32_t vigor;
    uint32_t attunement;
    uint32_t endurance;
	uint32_t vitality;
    uint32_t strength;
    uint32_t dexterity;
    uint32_t intelligence;
    uint32_t faith;
	uint32_t luck;
};

class DS3StatsReader {
private:
    MemoryReader reader;

    static constexpr uintptr_t GAMEDATAMAN_POINTER = 0x047572B8;
    static constexpr uintptr_t DEATH_COUNT_OFFSET = 0x98;
    static constexpr uintptr_t PLAYTIME_OFFSET = 0xA4;

    static constexpr uintptr_t WORLDCHRMAN_POINTER = 0x0477FDB8;
    static constexpr uintptr_t WORLDCHRMAN_PLAYER_OFFSET = 0x80;
    static constexpr uintptr_t PLAYER_ZONE_OFFSET = 0x1FE0;
    static constexpr uintptr_t PLAYER_PLAY_REGION_OFFSET = 0x1ABC;

    static constexpr uintptr_t BOSS_FIGHT_OFFSET = 0xC0;

    static constexpr uintptr_t PLAYER_DATA_OFFSET = 0x1F90;
    static constexpr uintptr_t PLAYER_HP_STRUCT_OFFSET = 0x18;
    static constexpr uintptr_t PLAYER_HP_OFFSET = 0xD8;

    static constexpr uintptr_t CHARACTER_DATA_OFFSET = 0x10;
    static constexpr uintptr_t CHARACTER_NAME_OFFSET = 0x88;
    static constexpr uintptr_t CHARACTER_LEVEL_OFFSET = 0x70;
    static constexpr uintptr_t CHARACTER_CLASS_OFFSET = 0xAE;

	static constexpr uintptr_t STAT_VIGOR_OFFSET = 0x44;
    static constexpr uintptr_t STAT_ATTUNEMENT_OFFSET = 0x48;
    static constexpr uintptr_t STAT_ENDURANCE_OFFSET = 0x4C;
    static constexpr uintptr_t STAT_VITALITY_OFFSET = 0x6C;
    static constexpr uintptr_t STAT_STRENGTH_OFFSET = 0x50;
    static constexpr uintptr_t STAT_DEXTERITY_OFFSET = 0x54;
    static constexpr uintptr_t STAT_INTELLIGENCE_OFFSET = 0x58;
    static constexpr uintptr_t STAT_FAITH_OFFSET = 0x5C;
    static constexpr uintptr_t STAT_LUCK_OFFSET = 0x60;

    static constexpr wchar_t PROCESS_NAME[] = L"DarkSoulsIII.exe";

    std::expected<uint32_t, MemoryReaderError> ReadGameData(uintptr_t basePointer, uintptr_t offset);
    std::expected<uint32_t, MemoryReaderError> ReadWorldChrData(uintptr_t offset);
    std::expected<uintptr_t, MemoryReaderError> GetCharacterDataBase();

public:
    std::expected<void, MemoryReaderError> Initialize();
    bool IsInitialized() const;
    bool IsProcessRunning() const;
    std::expected<uint32_t, MemoryReaderError> GetDeathCount();
    std::expected<uint32_t, MemoryReaderError> GetPlayTime();
    std::expected<uint32_t, MemoryReaderError> GetCurrentZone();
    std::expected<uint32_t, MemoryReaderError> GetPlayRegion();
    std::expected<bool, MemoryReaderError> GetInBossFight();
    std::expected<int32_t, MemoryReaderError> GetPlayerHP();

    std::expected<std::wstring, MemoryReaderError> GetCharacterName();
    std::expected<uint8_t, MemoryReaderError> GetClass();
	std::expected<CharacterStats, MemoryReaderError> GetCharacterStats();
};
