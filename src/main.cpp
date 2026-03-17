#include <windows.h>
#include <commctrl.h>
#include <gdiplus.h>

#include "app_state.h"
#include "monitor_manager.h"
#include "overlay_window.h"
#include "control_window.h"
#include "resource.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_BAR_CLASSES | ICC_UPDOWN_CLASS;
    InitCommonControlsEx(&icc);

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    LoadConfig(g_app);
    if (g_app.imagePath[0] != '\0')
    {
        wchar_t wPath[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, g_app.imagePath, -1, wPath, MAX_PATH);

        g_app.image = Gdiplus::Image::FromFile(wPath, FALSE);
        if (!g_app.image || g_app.image->GetLastStatus() != Gdiplus::Ok)
        {
            delete g_app.image;
            g_app.image = nullptr;
            g_app.imagePath[0] = '\0';
            g_app.useImage = false;
        }
        else
        {
            g_app.useImage = true;
        }
    }

    g_app.bgBrush = CreateSolidBrush(RGB(24, 24, 28));
    g_app.panelBrush = CreateSolidBrush(RGB(36, 36, 42));

    g_app.font = CreateFontA(
        -14, 0, 0, 0,
        FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        "Segoe UI"
    );

    g_monitors.Refresh();
    if (g_monitors.Count() > 0)
    {
        if (g_app.monitorIndex < 0 || g_app.monitorIndex >= g_monitors.Count())
            g_app.monitorIndex = g_monitors.GetPrimaryIndex();

        if (g_app.monitorIndex < 0)
            g_app.monitorIndex = 0;
    }
    else
    {
        g_app.monitorIndex = 0;
    }

    if (!RegisterOverlayWindowClass(hInstance))
        return 1;

    if (!RegisterControlWindowClass(hInstance))
        return 1;

    g_app.overlayHwnd = CreateOverlayWindow(hInstance);
    if (!g_app.overlayHwnd)
        return 1;

    g_app.controlHwnd = CreateControlWindow(hInstance, nCmdShow);
    if (!g_app.controlHwnd)
        return 1;

    RefreshOverlayNow();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_app.image)
    {
        delete g_app.image;
        g_app.image = nullptr;
    }
    Gdiplus::GdiplusShutdown(gdiplusToken);

    return 0;
}