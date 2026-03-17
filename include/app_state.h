#pragma once
#include <windows.h>
#include <gdiplus.h>

struct AppState
{
    HWND overlayHwnd = NULL;
    HWND controlHwnd = NULL;

    HFONT font = NULL;
    HBRUSH bgBrush = NULL;
    HBRUSH panelBrush = NULL;

    int monitorIndex = -1;

    int overlayX = 0;
    int overlayY = 0;
    int overlayW = 500;
    int overlayH = 100;

    int colorR = 200;
    int colorG = 0;
    int colorB = 200;
    int opacityPercent = 50;

    bool useImage = false;
    bool keepImageAspectRatio = true;
    char imagePath[MAX_PATH] = "";
    Gdiplus::Image* image = nullptr;
};

extern AppState g_app;

int ClampInt(int value, int minValue, int maxValue);
BYTE OpacityPercentToAlphaByte(int percent);

const char* GetIniFilePath();

void LoadConfig(AppState& app);
void SaveConfig(const AppState& app);