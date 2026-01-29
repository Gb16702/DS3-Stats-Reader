#pragma once

class AutoStart {
private:
    static constexpr const wchar_t* RUN_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    static constexpr const wchar_t* APP_NAME = L"Ember";

public:
    static bool Enable();
    static bool Disable();
};
