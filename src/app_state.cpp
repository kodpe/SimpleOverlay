#include "app_state.h"
#include <shlobj.h>
#include <cstdio>
#include <cstring>

AppState g_app;

int ClampInt(int value, int minValue, int maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

BYTE OpacityPercentToAlphaByte(int percent)
{
    percent = ClampInt(percent, 0, 100);
    return (BYTE)((percent * 255) / 100);
}

static char g_iniPath[MAX_PATH] = "";

const char* GetIniFilePath()
{
    if (g_iniPath[0] != '\0')
        return g_iniPath;

    char appData[MAX_PATH];

    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appData) != S_OK)
    {
        std::strcpy(g_iniPath, "SimpleOverlay.ini");
        return g_iniPath;
    }

    std::snprintf(
        g_iniPath,
        MAX_PATH,
        "%s\\SimpleOverlay",
        appData
    );

    CreateDirectoryA(g_iniPath, NULL);

    std::strcat(g_iniPath, "\\SimpleOverlay.ini");

    return g_iniPath;
}

void LoadConfig(AppState& app)
{
    app.monitorIndex    = GetPrivateProfileIntA("overlay", "monitor_index", app.monitorIndex, GetIniFilePath());

    app.overlayX        = GetPrivateProfileIntA("overlay", "x", app.overlayX, GetIniFilePath());
    app.overlayY        = GetPrivateProfileIntA("overlay", "y", app.overlayY, GetIniFilePath());
    app.overlayW        = GetPrivateProfileIntA("overlay", "w", app.overlayW, GetIniFilePath());
    app.overlayH        = GetPrivateProfileIntA("overlay", "h", app.overlayH, GetIniFilePath());

    app.colorR          = GetPrivateProfileIntA("overlay", "r", app.colorR, GetIniFilePath());
    app.colorG          = GetPrivateProfileIntA("overlay", "g", app.colorG, GetIniFilePath());
    app.colorB          = GetPrivateProfileIntA("overlay", "b", app.colorB, GetIniFilePath());
    app.opacityPercent  = GetPrivateProfileIntA("overlay", "opacity_percent", app.opacityPercent, GetIniFilePath());

    app.overlayW        = ClampInt(app.overlayW, 1, 10000);
    app.overlayH        = ClampInt(app.overlayH, 1, 10000);
    app.colorR          = ClampInt(app.colorR, 0, 255);
    app.colorG          = ClampInt(app.colorG, 0, 255);
    app.colorB          = ClampInt(app.colorB, 0, 255);
    app.opacityPercent  = ClampInt(app.opacityPercent, 0, 100);

    GetPrivateProfileStringA("overlay", "image_path", "", app.imagePath, MAX_PATH, GetIniFilePath());
    app.useImage = app.imagePath[0] != '\0';
    app.keepImageAspectRatio = GetPrivateProfileIntA("overlay", "keep_image_aspect_ratio", 1, GetIniFilePath()) != 0;
}

void SaveConfig(const AppState& app)
{
    char buf[64];

    std::snprintf(buf, sizeof(buf), "%d", app.monitorIndex);
    WritePrivateProfileStringA("overlay", "monitor_index", buf, GetIniFilePath());

    std::snprintf(buf, sizeof(buf), "%d", app.overlayX);
    WritePrivateProfileStringA("overlay", "x", buf, GetIniFilePath());

    std::snprintf(buf, sizeof(buf), "%d", app.overlayY);
    WritePrivateProfileStringA("overlay", "y", buf, GetIniFilePath());

    std::snprintf(buf, sizeof(buf), "%d", app.overlayW);
    WritePrivateProfileStringA("overlay", "w", buf, GetIniFilePath());

    std::snprintf(buf, sizeof(buf), "%d", app.overlayH);
    WritePrivateProfileStringA("overlay", "h", buf, GetIniFilePath());

    std::snprintf(buf, sizeof(buf), "%d", app.colorR);
    WritePrivateProfileStringA("overlay", "r", buf, GetIniFilePath());

    std::snprintf(buf, sizeof(buf), "%d", app.colorG);
    WritePrivateProfileStringA("overlay", "g", buf, GetIniFilePath());

    std::snprintf(buf, sizeof(buf), "%d", app.colorB);
    WritePrivateProfileStringA("overlay", "b", buf, GetIniFilePath());

    std::snprintf(buf, sizeof(buf), "%d", app.opacityPercent);
    WritePrivateProfileStringA("overlay", "opacity_percent", buf, GetIniFilePath());

    WritePrivateProfileStringA("overlay", "image_path", app.imagePath, GetIniFilePath());
    WritePrivateProfileStringA("overlay", "keep_image_aspect_ratio", app.keepImageAspectRatio ? "1" : "0", GetIniFilePath());

    // MessageBoxA(NULL, GetIniFilePath(), "INI path", MB_OK);
}