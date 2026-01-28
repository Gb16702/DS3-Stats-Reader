#pragma once

#include "MemoryReader.h"

#include <expected>
#include <cstdint>

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

    static constexpr wchar_t PROCESS_NAME[] = L"DarkSoulsIII.exe";

    std::expected<uint32_t, MemoryReaderError> ReadGameData(uintptr_t basePointer, uintptr_t offset);
    std::expected<uint32_t, MemoryReaderError> ReadWorldChrData(uintptr_t offset);

public:
    std::expected<void, MemoryReaderError> Initialize();
    bool IsInitialized() const;
    std::expected<uint32_t, MemoryReaderError> GetDeathCount();
    std::expected<uint32_t, MemoryReaderError> GetPlayTime();
    std::expected<uint32_t, MemoryReaderError> GetCurrentZone();
    std::expected<uint32_t, MemoryReaderError> GetPlayRegion();
    std::expected<bool, MemoryReaderError> GetInBossFight();
    std::expected<int32_t, MemoryReaderError> GetPlayerHP();
};
