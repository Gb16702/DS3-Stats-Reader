#include "DS3StatsReader.h"

std::expected<uint32_t, MemoryReaderError> DS3StatsReader::ReadGameData(uintptr_t basePointer, uintptr_t offset) {
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

std::expected<void, MemoryReaderError> DS3StatsReader::Initialize() {
    return reader.Initialize(PROCESS_NAME);
}

bool DS3StatsReader::IsInitialized() const {
    return reader.IsInitialized();
}

std::expected<uint32_t, MemoryReaderError> DS3StatsReader::GetDeathCount() {
    return ReadGameData(GAMEDATAMAN_POINTER, DEATH_COUNT_OFFSET);
}

std::expected<uint32_t, MemoryReaderError> DS3StatsReader::GetPlayTime() {
    return ReadGameData(GAMEDATAMAN_POINTER, PLAYTIME_OFFSET);
}

std::expected<uint32_t, MemoryReaderError> DS3StatsReader::ReadWorldChrData(uintptr_t offset) {
    uintptr_t pointerAddress = reader.GetModuleBase() + WORLDCHRMAN_POINTER;

    uintptr_t worldChrMan = 0;
    if (!reader.ReadMemory(pointerAddress, worldChrMan)) {
        return std::unexpected(MemoryReaderError::ReadFailed);
    }

    if (worldChrMan == 0) {
        return std::unexpected(MemoryReaderError::ReadFailed);
    }

    uintptr_t playerPtr = 0;
    if (!reader.ReadMemory(worldChrMan + WORLDCHRMAN_PLAYER_OFFSET, playerPtr)) {
        return std::unexpected(MemoryReaderError::ReadFailed);
    }

    if (playerPtr == 0) {
        return std::unexpected(MemoryReaderError::ReadFailed);
    }

    uint32_t value = 0;
    if (!reader.ReadMemory(playerPtr + offset, value)) {
        return std::unexpected(MemoryReaderError::ReadFailed);
    }

    return value;
}

std::expected<uint32_t, MemoryReaderError> DS3StatsReader::GetCurrentZone() {
    return ReadWorldChrData(PLAYER_ZONE_OFFSET);
}

std::expected<uint32_t, MemoryReaderError> DS3StatsReader::GetPlayRegion() {
    return ReadWorldChrData(PLAYER_PLAY_REGION_OFFSET);
}

std::expected<bool, MemoryReaderError> DS3StatsReader::GetInBossFight() {
    auto result = ReadGameData(GAMEDATAMAN_POINTER, BOSS_FIGHT_OFFSET);
    if (!result) {
        return std::unexpected(result.error());
    }
    return *result != 0;
}

std::expected<int32_t, MemoryReaderError> DS3StatsReader::GetPlayerHP() {
    uintptr_t pointerAddress = reader.GetModuleBase() + WORLDCHRMAN_POINTER;

    uintptr_t worldChrMan = 0;
    if (!reader.ReadMemory(pointerAddress, worldChrMan) || worldChrMan == 0) {
        return std::unexpected(MemoryReaderError::ReadFailed);
    }

    uintptr_t playerPtr = 0;
    if (!reader.ReadMemory(worldChrMan + WORLDCHRMAN_PLAYER_OFFSET, playerPtr) || playerPtr == 0) {
        return std::unexpected(MemoryReaderError::ReadFailed);
    }

    uintptr_t playerData = 0;
    if (!reader.ReadMemory(playerPtr + PLAYER_DATA_OFFSET, playerData) || playerData == 0) {
        return std::unexpected(MemoryReaderError::ReadFailed);
    }

    uintptr_t hpStruct = 0;
    if (!reader.ReadMemory(playerData + PLAYER_HP_STRUCT_OFFSET, hpStruct) || hpStruct == 0) {
        return std::unexpected(MemoryReaderError::ReadFailed);
    }

    int32_t hp = 0;
    if (!reader.ReadMemory(hpStruct + PLAYER_HP_OFFSET, hp)) {
        return std::unexpected(MemoryReaderError::ReadFailed);
    }

    return hp;
}

std::expected<uintptr_t, MemoryReaderError> DS3StatsReader::GetCharacterDataBase() {
    uintptr_t pointerAddress = reader.GetModuleBase() + GAMEDATAMAN_POINTER;

    uintptr_t gameDataMan = 0;
    if (!reader.ReadMemory(pointerAddress, gameDataMan) || gameDataMan == 0) {
        return std::unexpected(MemoryReaderError::ReadFailed);
    }

    uintptr_t characterData = 0;
    if (!reader.ReadMemory(gameDataMan + CHARACTER_DATA_OFFSET, characterData) || characterData == 0) {
        return std::unexpected(MemoryReaderError::ReadFailed);
    }

    return characterData;
}

std::expected<std::wstring, MemoryReaderError> DS3StatsReader::GetCharacterName() {
    auto baseResult = GetCharacterDataBase();
    if (!baseResult) {
        return std::unexpected(baseResult.error());
    }

    wchar_t nameBuffer[24] = {0};
    if (!reader.ReadMemory(*baseResult + CHARACTER_NAME_OFFSET, nameBuffer)) {
        return std::unexpected(MemoryReaderError::ReadFailed);
    }

    return std::wstring(nameBuffer);
}

std::expected<uint32_t, MemoryReaderError> DS3StatsReader::GetSoulLevel() {
    auto baseResult = GetCharacterDataBase();
    if (!baseResult) {
        return std::unexpected(baseResult.error());
    }

    uint32_t level = 0;
    if (!reader.ReadMemory(*baseResult + CHARACTER_LEVEL_OFFSET, level)) {
        return std::unexpected(MemoryReaderError::ReadFailed);
    }

    return level;
}

std::expected<uint8_t, MemoryReaderError> DS3StatsReader::GetClass() {
    auto baseResult = GetCharacterDataBase();
    if (!baseResult) {
        return std::unexpected(baseResult.error());
    }

    uint8_t classId = 0;
    if (!reader.ReadMemory(*baseResult + CHARACTER_CLASS_OFFSET, classId)) {
        return std::unexpected(MemoryReaderError::ReadFailed);
    }

    return classId;
}
