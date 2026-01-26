#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class BorderlessWindow {
private:
    static constexpr wchar_t WINDOW_TITLE[] = L"DARK SOULS III";

    HWND windowHandle = nullptr;
    LONG windowedStyle = 0;
    RECT windowedRect = {};
    bool isActive = false;

    bool FindGameWindow();

public:
    BorderlessWindow() = default;

    BorderlessWindow(const BorderlessWindow&) = delete;
    BorderlessWindow& operator=(const BorderlessWindow&) = delete;

    bool Enable();
    bool Disable();
    bool IsActive() const;
};

extern BorderlessWindow g_borderlessWindow;
