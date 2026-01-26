#include "AutoStart.h"
#include "../core/Log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

bool AutoStart::Enable() {
    wchar_t exePath[MAX_PATH];
    if (::GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) {
        log(LogLevel::ERR, "Failed to get executable path");
        return false;
    }

    HKEY hkey;
    LONG result = ::RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &hkey);

    if (result != ERROR_SUCCESS) {
        log(LogLevel::ERR, "Failed to open registry key");
        return false;
    }

    result = ::RegSetValueExW(hkey, APP_NAME, 0, REG_SZ, reinterpret_cast<const BYTE*>(exePath), static_cast<DWORD>((wcslen(exePath) + 1) * sizeof(wchar_t)));
    ::RegCloseKey(hkey);

    if (result != ERROR_SUCCESS) {
        log(LogLevel::ERR, "Failed to set registry value");
        return false;
    }

    log(LogLevel::INFO, "Auto-start enabled");

    return true;
}

bool AutoStart::Disable() {
    HKEY hkey;
    LONG result = ::RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &hkey);

    if (result != ERROR_SUCCESS) {
        log(LogLevel::ERR, "Failed to open registry key");
        return false;
    }

    result = ::RegDeleteValueW(hkey, APP_NAME);
    ::RegCloseKey(hkey);

    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
        log(LogLevel::ERR, "Failed to delete registry value");
        return false;
    }

    log(LogLevel::INFO, "Auto-start disabled");
    return true;
}
