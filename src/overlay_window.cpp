#include "overlay_window.h"
#include "app_state.h"
#include "monitor_manager.h"

#include <gdiplus.h>
using namespace Gdiplus;

static const char OVERLAY_CLASS[] = "OverlayWindowClassKodpMulti";

static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_NCHITTEST:
        return HTTRANSPARENT;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &rcClient, bg);
        DeleteObject(bg);

        if (g_app.useImage && g_app.image)
        {
            Graphics graphics(hdc);
            graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            graphics.DrawImage(
                g_app.image,
                Rect(0, 0, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top)
            );
        }
        else
        {
            HBRUSH brush = CreateSolidBrush(RGB(g_app.colorR, g_app.colorG, g_app.colorB));
            FillRect(hdc, &rcClient, brush);
            DeleteObject(brush);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

bool RegisterOverlayWindowClass(HINSTANCE hInstance)
{
    WNDCLASSA wc{};
    wc.lpfnWndProc = OverlayProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = OVERLAY_CLASS;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    return RegisterClassA(&wc) != 0;
}

HWND CreateOverlayWindow(HINSTANCE hInstance)
{
    HWND hwnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        OVERLAY_CLASS,
        "Simple Overlay",
        WS_POPUP,
        0, 0, 100, 100,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd)
    {
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        UpdateWindow(hwnd);
    }

    return hwnd;
}

void ApplyOverlayVisuals()
{
    if (!g_app.overlayHwnd)
        return;

    SetLayeredWindowAttributes(
        g_app.overlayHwnd,
        0,
        OpacityPercentToAlphaByte(g_app.opacityPercent),
        LWA_ALPHA
    );

    InvalidateRect(g_app.overlayHwnd, NULL, TRUE);
}

void ApplyOverlayPlacement()
{
    if (!g_app.overlayHwnd)
        return;

    const MonitorInfo* mon = g_monitors.Get(g_app.monitorIndex);
    if (!mon)
        return;

    const int screenW = mon->rcMonitor.right - mon->rcMonitor.left;
    const int screenH = mon->rcMonitor.bottom - mon->rcMonitor.top;

    g_app.overlayW = ClampInt(g_app.overlayW, 1, screenW);
    g_app.overlayH = ClampInt(g_app.overlayH, 1, screenH);

    g_app.overlayX = ClampInt(g_app.overlayX, 0, screenW - g_app.overlayW);
    g_app.overlayY = ClampInt(g_app.overlayY, 0, screenH - g_app.overlayH);

    const int absX = mon->rcMonitor.left + g_app.overlayX;
    const int absY = mon->rcMonitor.top + g_app.overlayY;

    SetWindowPos(
        g_app.overlayHwnd,
        HWND_TOPMOST,
        absX,
        absY,
        g_app.overlayW,
        g_app.overlayH,
        SWP_NOACTIVATE | SWP_SHOWWINDOW
    );
}

void EnsureControlAboveOverlay()
{
    if (!g_app.overlayHwnd || !g_app.controlHwnd)
        return;

    SetWindowPos(
        g_app.controlHwnd,
        HWND_TOPMOST,
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
    );

    SetWindowPos(
        g_app.overlayHwnd,
        g_app.controlHwnd,
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
    );
}

void RefreshOverlayNow()
{
    ApplyOverlayPlacement();
    ApplyOverlayVisuals();
    EnsureControlAboveOverlay();
    SaveConfig(g_app);
}