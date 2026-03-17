#include "control_window.h"
#include "ids.h"
#include "app_state.h"
#include "monitor_manager.h"
#include "overlay_window.h"
#include "resource.h"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static bool g_isSyncingUi = false;

static const char CONTROL_CLASS[] = "OverlayControlWindowClassKodpMulti";
static const char PREVIEW_CLASS[] = "OverlayPreviewWindowClassKodpMulti";

static const COLORREF COLOR_BG     = RGB(24, 24, 28);
static const COLORREF COLOR_PANEL  = RGB(36, 36, 42);
static const COLORREF COLOR_TEXT   = RGB(235, 235, 235);
static const COLORREF COLOR_BORDER = RGB(70, 70, 80);

struct ControlWidgets
{
    HWND comboMonitor = NULL;

    // position
    HWND editX = NULL;
    HWND editY = NULL;
    HICON iconCornerTL = NULL;
    HICON iconCornerTR = NULL;
    HICON iconCornerBL = NULL;
    HICON iconCornerBR = NULL;

    // taille
    HWND editW = NULL;
    HWND editH = NULL;
    HWND editWPercent = NULL;
    HWND editHPercent = NULL;

    // color
    HWND sliderOpacity = NULL;
    HWND valueOpacity = NULL;
    HWND preview = NULL;

    // image
    HWND btnBrowseImage = NULL;
    HWND btnClearImage = NULL;
    HWND btnFitImage = NULL;
    HWND labelImagePath = NULL;
    HWND checkKeepImageRatio = NULL;
};

static ControlWidgets g_ui;

static bool GetCurrentMonitorSize(int& screenW, int& screenH)
{
    const MonitorInfo* mon = g_monitors.Get(g_app.monitorIndex);
    if (!mon)
        return false;

    screenW = mon->rcMonitor.right - mon->rcMonitor.left;
    screenH = mon->rcMonitor.bottom - mon->rcMonitor.top;
    return true;
}

static int PixelsToPercent(int pixels, int total)
{
    if (total <= 0)
        return 0;

    return (pixels * 100 + total / 2) / total;
}

static int PercentToPixels(int percent, int total)
{
    percent = ClampInt(percent, 0, 100);
    if (total <= 0)
        return 1;

    int px = (percent * total + 50) / 100;
    return (px < 1) ? 1 : px;
}

static void SetEditInt(HWND hEdit, int value)
{
    char buf[32];
    char current[32];

    std::snprintf(buf, sizeof(buf), "%d", value);
    GetWindowTextA(hEdit, current, sizeof(current));

    if (std::strcmp(buf, current) != 0)
        SetWindowTextA(hEdit, buf);
}

static int GetEditInt(HWND hEdit)
{
    char buf[32];
    GetWindowTextA(hEdit, buf, sizeof(buf));
    return std::atoi(buf);
}

static int GetSliderValue(HWND hSlider)
{
    return (int)SendMessage(hSlider, TBM_GETPOS, 0, 0);
}

static void SetSliderValue(HWND hSlider, int value)
{
    SendMessage(hSlider, TBM_SETPOS, TRUE, value);
}

static void SetLabelInt(HWND hLabel, int value, const char* suffix = "")
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d%s", value, suffix);
    SetWindowTextA(hLabel, buf);
}

static void ApplyFont(HWND hwnd)
{
    if (g_app.font && hwnd)
        SendMessage(hwnd, WM_SETFONT, (WPARAM)g_app.font, TRUE);
}

static void ApplyFontToChildren(HWND parent)
{
    HWND child = GetWindow(parent, GW_CHILD);
    while (child)
    {
        ApplyFont(child);
        child = GetWindow(child, GW_HWNDNEXT);
    }
}

static void SyncUiFromState();

static void OpenColorPicker(HWND hwnd)
{
    static COLORREF customColors[16] = { 0 };

    CHOOSECOLORA cc{};
    cc.lStructSize = sizeof(CHOOSECOLORA);
    cc.hwndOwner = hwnd;
    cc.lpCustColors = customColors;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;

    cc.rgbResult = RGB(
        g_app.colorR,
        g_app.colorG,
        g_app.colorB
    );

    if (ChooseColorA(&cc))
    {
        COLORREF c = cc.rgbResult;

        g_app.colorR = GetRValue(c);
        g_app.colorG = GetGValue(c);
        g_app.colorB = GetBValue(c);

        SyncUiFromState();
        RefreshOverlayNow();
    }
}

static HWND CreateSectionIcon(HWND parent, HINSTANCE hInstance, int x, int y, int resourceId)
{
    HWND hIconCtrl = CreateWindowA(
        "STATIC",
        "",
        WS_CHILD | WS_VISIBLE | SS_ICON | SS_CENTERIMAGE,
        x, y, 20, 20,
        parent,
        NULL,
        hInstance,
        NULL
    );

    HICON hIcon = (HICON)LoadImage(
        hInstance,
        MAKEINTRESOURCE(resourceId),
        IMAGE_ICON,
        16, 16,
        LR_DEFAULTCOLOR
    );

    if (hIcon)
        SendMessage(hIconCtrl, STM_SETICON, (WPARAM)hIcon, 0);

    return hIconCtrl;
}

static HWND CreateIconButton(HWND parent, HINSTANCE hInstance, int x, int y, int id)
{
    return CreateWindowA(
        "BUTTON", "",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x, y, 24, 24,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        hInstance,
        NULL
    );
}

static void FillMonitorCombo()
{
    SendMessageA(g_ui.comboMonitor, CB_RESETCONTENT, 0, 0);

    for (int i = 0; i < g_monitors.Count(); ++i)
    {
        const MonitorInfo* mon = g_monitors.Get(i);
        if (!mon)
            continue;

        SendMessageA(g_ui.comboMonitor, CB_ADDSTRING, 0, (LPARAM)mon->displayName.c_str());
    }

    if (g_monitors.Count() <= 0)
        return;

    g_app.monitorIndex = ClampInt(g_app.monitorIndex, 0, g_monitors.Count() - 1);
    SendMessageA(g_ui.comboMonitor, CB_SETCURSEL, g_app.monitorIndex, 0);
}

static void UpdatePreview()
{
    SetLabelInt(g_ui.valueOpacity, g_app.opacityPercent, "%");

    if (g_ui.preview)
        InvalidateRect(g_ui.preview, NULL, TRUE);
}

static void UpdateImageUi();

static void SyncUiFromState()
{
    g_isSyncingUi = true;

    FillMonitorCombo();

    SetEditInt(g_ui.editX, g_app.overlayX);
    SetEditInt(g_ui.editY, g_app.overlayY);
    SetEditInt(g_ui.editW, g_app.overlayW);
    SetEditInt(g_ui.editH, g_app.overlayH);

    int screenW = 0;
    int screenH = 0;
    if (GetCurrentMonitorSize(screenW, screenH))
    {
        SetEditInt(g_ui.editWPercent, PixelsToPercent(g_app.overlayW, screenW));
        SetEditInt(g_ui.editHPercent, PixelsToPercent(g_app.overlayH, screenH));
    }
    else
    {
        SetEditInt(g_ui.editWPercent, 0);
        SetEditInt(g_ui.editHPercent, 0);
    }

    SetSliderValue(g_ui.sliderOpacity, g_app.opacityPercent);

    UpdatePreview();
    UpdateImageUi();

    if (g_ui.checkKeepImageRatio)
    {
        SendMessage(
            g_ui.checkKeepImageRatio,
            BM_SETCHECK,
            g_app.keepImageAspectRatio ? BST_CHECKED : BST_UNCHECKED,
            0
        );
    }

    g_isSyncingUi = false;
}

static void RebindPositionRanges()
{
    const MonitorInfo* mon = g_monitors.Get(g_app.monitorIndex);
    if (!mon)
        return;

    const int screenW = mon->rcMonitor.right - mon->rcMonitor.left;
    const int screenH = mon->rcMonitor.bottom - mon->rcMonitor.top;

    g_app.overlayW = ClampInt(g_app.overlayW, 1, screenW);
    g_app.overlayH = ClampInt(g_app.overlayH, 1, screenH);

    g_app.overlayX = ClampInt(g_app.overlayX, 0, screenW - g_app.overlayW);
    g_app.overlayY = ClampInt(g_app.overlayY, 0, screenH - g_app.overlayH);
}

static void ApplyImageAspectFromWidth(int imgW, int imgH, int& overlayW, int& overlayH)
{
    if (imgW <= 0 || imgH <= 0 || overlayW <= 0)
        return;

    overlayH = (overlayW * imgH + imgW / 2) / imgW;
    if (overlayH < 1)
        overlayH = 1;
}

static void ApplyImageAspectFromHeight(int imgW, int imgH, int& overlayW, int& overlayH)
{
    if (imgW <= 0 || imgH <= 0 || overlayH <= 0)
        return;

    overlayW = (overlayH * imgW + imgH / 2) / imgH;
    if (overlayW < 1)
        overlayW = 1;
}

static void ApplyImageAspectFromWidthBounded(
    int imgW, int imgH,
    int screenW, int screenH,
    int& overlayW, int& overlayH)
{
    overlayW = ClampInt(overlayW, 1, screenW);
    ApplyImageAspectFromWidth(imgW, imgH, overlayW, overlayH);

    if (overlayH > screenH)
    {
        overlayH = screenH;
        ApplyImageAspectFromHeight(imgW, imgH, overlayW, overlayH);
    }

    overlayW = ClampInt(overlayW, 1, screenW);
    overlayH = ClampInt(overlayH, 1, screenH);
}

static void ApplyImageAspectFromHeightBounded(
    int imgW, int imgH,
    int screenW, int screenH,
    int& overlayW, int& overlayH)
{
    overlayH = ClampInt(overlayH, 1, screenH);
    ApplyImageAspectFromHeight(imgW, imgH, overlayW, overlayH);

    if (overlayW > screenW)
    {
        overlayW = screenW;
        ApplyImageAspectFromWidth(imgW, imgH, overlayW, overlayH);
    }

    overlayW = ClampInt(overlayW, 1, screenW);
    overlayH = ClampInt(overlayH, 1, screenH);
}

static void ApplyAllLive(int changedId)
{
    g_app.monitorIndex = (int)SendMessageA(g_ui.comboMonitor, CB_GETCURSEL, 0, 0);
    if (g_app.monitorIndex < 0)
        g_app.monitorIndex = 0;

    int screenW = 0;
    int screenH = 0;
    GetCurrentMonitorSize(screenW, screenH);

    int requestedW = g_app.overlayW;
    int requestedH = g_app.overlayH;

    if (changedId == ID_EDIT_W_PERCENT)
    {
        int pct = GetEditInt(g_ui.editWPercent);
        pct = ClampInt(pct, 0, 100);
        requestedW = PercentToPixels(pct, screenW);
    }
    else
    {
        requestedW = GetEditInt(g_ui.editW);
        if (requestedW < 1)
            requestedW = 1;
    }

    if (changedId == ID_EDIT_H_PERCENT)
    {
        int pct = GetEditInt(g_ui.editHPercent);
        pct = ClampInt(pct, 0, 100);
        requestedH = PercentToPixels(pct, screenH);
    }
    else
    {
        requestedH = GetEditInt(g_ui.editH);
        if (requestedH < 1)
            requestedH = 1;
    }

    g_app.overlayW = requestedW;
    g_app.overlayH = requestedH;

    if (g_app.useImage && g_app.image && g_app.keepImageAspectRatio)
    {
        const int imgW = (int)g_app.image->GetWidth();
        const int imgH = (int)g_app.image->GetHeight();

        if (changedId == ID_EDIT_W || changedId == ID_EDIT_W_PERCENT)
        {
            ApplyImageAspectFromWidthBounded(
                imgW, imgH,
                screenW, screenH,
                g_app.overlayW, g_app.overlayH
            );
        }
        else if (changedId == ID_EDIT_H || changedId == ID_EDIT_H_PERCENT)
        {
            ApplyImageAspectFromHeightBounded(
                imgW, imgH,
                screenW, screenH,
                g_app.overlayW, g_app.overlayH
            );
        }
        else
        {
            g_app.overlayW = ClampInt(g_app.overlayW, 1, screenW);
            g_app.overlayH = ClampInt(g_app.overlayH, 1, screenH);
        }
    }
    else
    {
        RebindPositionRanges();
    }

    g_app.overlayX = GetEditInt(g_ui.editX);
    g_app.overlayY = GetEditInt(g_ui.editY);

    if (screenW > 0 && screenH > 0)
    {
        g_app.overlayX = ClampInt(g_app.overlayX, 0, screenW - g_app.overlayW);
        g_app.overlayY = ClampInt(g_app.overlayY, 0, screenH - g_app.overlayH);
    }

    g_app.opacityPercent = ClampInt(GetSliderValue(g_ui.sliderOpacity), 0, 100);

    SyncUiFromState();
    RefreshOverlayNow();
}

static void ClearImageSelection();
static void ResetOverlay()
{
    g_monitors.Refresh();

    int primaryIndex = g_monitors.GetPrimaryIndex();
    if (primaryIndex >= 0)
        g_app.monitorIndex = primaryIndex;
    else
        g_app.monitorIndex = 0;

    g_app.overlayX = 0;
    g_app.overlayY = 0;
    g_app.overlayW = 500;
    g_app.overlayH = 100;

    g_app.colorR = 200;
    g_app.colorG = 0;
    g_app.colorB = 200;
    g_app.opacityPercent = 50;

    ClearImageSelection();
    RebindPositionRanges();
    SyncUiFromState();
    RefreshOverlayNow();
}

static void MoveOverlayToCorner(int cornerId)
{
    const MonitorInfo* mon = g_monitors.Get(g_app.monitorIndex);
    if (!mon)
        return;

    const int screenW = mon->rcMonitor.right - mon->rcMonitor.left;
    const int screenH = mon->rcMonitor.bottom - mon->rcMonitor.top;

    g_app.overlayW = ClampInt(g_app.overlayW, 1, screenW);
    g_app.overlayH = ClampInt(g_app.overlayH, 1, screenH);

    switch (cornerId)
    {
    case ID_BTN_CORNER_TL:
        g_app.overlayX = 0;
        g_app.overlayY = 0;
        break;

    case ID_BTN_CORNER_TR:
        g_app.overlayX = screenW - g_app.overlayW;
        g_app.overlayY = 0;
        break;

    case ID_BTN_CORNER_BL:
        g_app.overlayX = 0;
        g_app.overlayY = screenH - g_app.overlayH;
        break;

    case ID_BTN_CORNER_BR:
        g_app.overlayX = screenW - g_app.overlayW;
        g_app.overlayY = screenH - g_app.overlayH;
        break;
    }

    SyncUiFromState();
    RefreshOverlayNow();
}

static LRESULT CALLBACK PreviewProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)wParam;
    (void)lParam;

    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_LBUTTONDOWN:
        OpenColorPicker(GetParent(hwnd));
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);

        HBRUSH bg = CreateSolidBrush(COLOR_PANEL);
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        RECT inner = rc;
        InflateRect(&inner, -5, -5);

        HBRUSH fill = CreateSolidBrush(RGB(g_app.colorR, g_app.colorG, g_app.colorB));
        FillRect(hdc, &inner, fill);
        DeleteObject(fill);

        HPEN pen = CreatePen(PS_SOLID, 1, COLOR_BORDER);
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));

        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);

        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COLOR_TEXT);
        if (g_app.font)
            SelectObject(hdc, g_app.font);

        char text[128];
        std::snprintf(
            text, sizeof(text),
            "RGB %d, %d, %d",
            g_app.colorR, g_app.colorG, g_app.colorB
        );

        DrawTextA(hdc, text, -1, &inner, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static void DrawVerticalSeparator(HDC hdc, int x, int yTop, int yBottom)
{
    HPEN pen = CreatePen(PS_SOLID, 1, COLOR_BORDER);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);

    MoveToEx(hdc, x, yTop, NULL);
    LineTo(hdc, x, yBottom);

    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static void UpdateImageUi()
{
    if (!g_ui.labelImagePath)
        return;

    if (g_app.useImage && g_app.imagePath[0] != '\0')
    {
        const char* filename = strrchr(g_app.imagePath, '\\');
        filename = filename ? filename + 1 : g_app.imagePath;
        SetWindowTextA(g_ui.labelImagePath, filename);
    }
    else
    {
        SetWindowTextA(g_ui.labelImagePath, "No image");
    }
}

static void BrowseImageFile(HWND hwnd)
{
    char path[MAX_PATH] = "";

    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;

    ofn.lpstrFilter =
        "Images (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0"
        "All files (*.*)\0*.*\0";

    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameA(&ofn))
        return;

    // libere ancienne image
    if (g_app.image)
    {
        delete g_app.image;
        g_app.image = nullptr;
    }

    // conversion char to wchar pour GDI+
    wchar_t wpath[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, MAX_PATH);

    g_app.image = Gdiplus::Image::FromFile(wpath);

    if (!g_app.image || g_app.image->GetLastStatus() != Gdiplus::Ok)
    {
        MessageBoxA(hwnd, "Failed to load image.", "Error", MB_ICONERROR);
        return;
    }

    strcpy_s(g_app.imagePath, path);
    g_app.useImage = true;

    const char* filename = strrchr(path, '\\');
    filename = filename ? filename + 1 : path;

    SetWindowTextA(g_ui.labelImagePath, filename);

    SyncUiFromState();
    RefreshOverlayNow();
}

static void ClearImageSelection()
{
    if (g_app.image)
    {
        delete g_app.image;
        g_app.image = nullptr;
    }

    g_app.useImage = false;
    g_app.imagePath[0] = '\0';

    SetWindowTextA(g_ui.labelImagePath, "No image");

    RefreshOverlayNow();
}

static void FitOverlayToImage(HWND hwnd)
{
    if (!g_app.image)
        return;

    int screenW = 0;
    int screenH = 0;
    if (!GetCurrentMonitorSize(screenW, screenH))
        return;

    const int imgW = (int)g_app.image->GetWidth();
    const int imgH = (int)g_app.image->GetHeight();

    g_app.overlayW = imgW;
    g_app.overlayH = imgH;

    ApplyImageAspectFromWidthBounded(
        imgW, imgH,
        screenW, screenH,
        g_app.overlayW, g_app.overlayH
    );

    g_app.overlayX = ClampInt(g_app.overlayX, 0, screenW - g_app.overlayW);
    g_app.overlayY = ClampInt(g_app.overlayY, 0, screenH - g_app.overlayH);

    SyncUiFromState();
    RefreshOverlayNow();
    (void)hwnd;
}

static LRESULT CALLBACK ControlProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_app.bgBrush);

        const int margin = 10;
        const int colGap = 10;
        const int colW = 170;
        const int xCol1 = margin;
        const int xCol2 = xCol1 + colW + colGap;
        // const int xCol3 = xCol2 + colW + colGap;

        const int sep1X = xCol1 + colW + colGap / 2;
        const int sep2X = xCol2 + colW + colGap / 2;

        // DrawVerticalSeparator(hdc, sep1X, 10, 240);
        // DrawVerticalSeparator(hdc, sep2X, 12, 240);
        (void)sep1X;
        (void)sep2X;

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, COLOR_TEXT);
        SetBkColor(hdc, COLOR_BG);
        return (LRESULT)g_app.bgBrush;
    }

    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, COLOR_TEXT);
        SetBkColor(hdc, COLOR_PANEL);
        return (LRESULT)g_app.panelBrush;
    }

    case WM_CTLCOLORBTN:
    {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, COLOR_TEXT);
        SetBkColor(hdc, COLOR_PANEL);
        return (LRESULT)g_app.panelBrush;
    }

    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (!dis)
            return FALSE;

        if (dis->CtlID == ID_BTN_CORNER_TL ||
            dis->CtlID == ID_BTN_CORNER_TR ||
            dis->CtlID == ID_BTN_CORNER_BL ||
            dis->CtlID == ID_BTN_CORNER_BR)
        {
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;

            COLORREF bg = COLOR_PANEL;
            COLORREF border = COLOR_BORDER;

            if (dis->itemState & ODS_SELECTED)
                bg = RGB(52, 52, 60);
            else if (dis->itemState & ODS_HOTLIGHT)
                bg = RGB(44, 44, 50);

            HBRUSH brush = CreateSolidBrush(bg);
            FillRect(hdc, &rc, brush);
            DeleteObject(brush);

            HPEN pen = CreatePen(PS_SOLID, 1, border);
            HPEN oldPen = (HPEN)SelectObject(hdc, pen);
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(pen);

            HICON icon = NULL;

            switch (dis->CtlID)
            {
                case ID_BTN_CORNER_TL: icon = g_ui.iconCornerTL; break;
                case ID_BTN_CORNER_TR: icon = g_ui.iconCornerTR; break;
                case ID_BTN_CORNER_BL: icon = g_ui.iconCornerBL; break;
                case ID_BTN_CORNER_BR: icon = g_ui.iconCornerBR; break;
            }
            if (icon)
            {
                const int iconX = rc.left + ((rc.right - rc.left) - 24) / 2;
                const int iconY = rc.top + ((rc.bottom - rc.top) - 24) / 2;
                DrawIconEx(hdc, iconX, iconY, icon, 24, 24, 0, NULL, DI_NORMAL);
            }

            if (dis->itemState & ODS_FOCUS)
            {
                RECT focus = rc;
                InflateRect(&focus, -3, -3);
                DrawFocusRect(hdc, &focus);
            }

            return TRUE;
        }

        break;
    }

    case WM_CREATE:
    {
        HINSTANCE hInstance = ((LPCREATESTRUCT)lParam)->hInstance;
        g_app.controlHwnd = hwnd;

        const int margin = 16;
        const int colGap = 16;
        const int colW = 170;
        const int xCol1 = margin;
        const int xCol2 = xCol1 + colW + colGap;
        const int xCol3 = xCol2 + colW + colGap;
        const int yRow1 = 10;
        const int yRow2 = 150;
        const int skipIconX = 24;
        const int sizeXform = 60;
        const int sizeYform = 24;
        const int yStep = 30;
        const int yStepTxt = 34;
        const int screenComboW = 150;

        // SCREEN
        CreateSectionIcon(hwnd, hInstance, xCol1, yRow1, IDI_ICON_SCREEN);
        CreateWindowA("STATIC", "Screen", WS_CHILD | WS_VISIBLE,
            xCol1 + skipIconX, yRow1, 120, 18, hwnd, NULL, hInstance, NULL);

        g_ui.comboMonitor = CreateWindowExA(0, "COMBOBOX", "",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            xCol1, yRow1 + yStep, screenComboW, 220, hwnd, (HMENU)ID_COMBO_MONITOR, hInstance, NULL);

        // POSITION
        CreateSectionIcon(hwnd, hInstance, xCol1, yRow2, IDI_ICON_POSITION);
        CreateWindowA("STATIC", "Position", WS_CHILD | WS_VISIBLE,
            xCol1 + skipIconX, yRow2, 120, 18, hwnd, NULL, hInstance, NULL);

        CreateWindowA("STATIC", "X", WS_CHILD | WS_VISIBLE,
            xCol1 + 2, yRow2 + yStepTxt, 20, 18, hwnd, NULL, hInstance, NULL);
        g_ui.editX = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL | ES_RIGHT,
            xCol1 + skipIconX, yRow2 + yStep, sizeXform, sizeYform, hwnd, (HMENU)ID_EDIT_X, hInstance, NULL);

        CreateWindowA("STATIC", "Y", WS_CHILD | WS_VISIBLE,
            xCol1 + 2, yRow2 + yStep+yStepTxt, 20, 18, hwnd, NULL, hInstance, NULL);
        g_ui.editY = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL | ES_RIGHT,
            xCol1 + skipIconX, yRow2 + yStep*2, sizeXform, sizeYform, hwnd, (HMENU)ID_EDIT_Y, hInstance, NULL);

            g_ui.iconCornerTL = (HICON)LoadImage(
            hInstance,
            MAKEINTRESOURCE(IDI_ICON_CORNER_TL),
            IMAGE_ICON,
            24, 24,
            LR_DEFAULTCOLOR
        );

        g_ui.iconCornerTR = (HICON)LoadImage(
            hInstance,
            MAKEINTRESOURCE(IDI_ICON_CORNER_TR),
            IMAGE_ICON,
            24, 24,
            LR_DEFAULTCOLOR
        );

        g_ui.iconCornerBL = (HICON)LoadImage(
            hInstance,
            MAKEINTRESOURCE(IDI_ICON_CORNER_BL),
            IMAGE_ICON,
            24, 24,
            LR_DEFAULTCOLOR
        );

        g_ui.iconCornerBR = (HICON)LoadImage(
            hInstance,
            MAKEINTRESOURCE(IDI_ICON_CORNER_BR),
            IMAGE_ICON,
            24, 24,
            LR_DEFAULTCOLOR
        );

        CreateIconButton(hwnd, hInstance, xCol1 + 100,  yRow2 + yStep, ID_BTN_CORNER_TL);
        CreateIconButton(hwnd, hInstance, xCol1 + 130, yRow2 + yStep, ID_BTN_CORNER_TR);
        CreateIconButton(hwnd, hInstance, xCol1 + 100, yRow2 + yStep*2, ID_BTN_CORNER_BL);
        CreateIconButton(hwnd, hInstance, xCol1 + 130, yRow2 + yStep*2, ID_BTN_CORNER_BR);

        // SIZE
        CreateSectionIcon(hwnd, hInstance, xCol2, yRow2, IDI_ICON_SIZE);
        CreateWindowA("STATIC", "Size", WS_CHILD | WS_VISIBLE,
            xCol2 + skipIconX, yRow2, 120, 18, hwnd, NULL, hInstance, NULL);

        CreateWindowA("STATIC", "L", WS_CHILD | WS_VISIBLE,
            xCol2, yRow2 + yStepTxt, 20, 18, hwnd, NULL, hInstance, NULL);

        g_ui.editW = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL | ES_RIGHT,
            xCol2 + skipIconX, yRow2 + yStep, sizeXform, sizeYform, hwnd, (HMENU)ID_EDIT_W, hInstance, NULL);

        g_ui.editWPercent = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL | ES_RIGHT,
            xCol2 + skipIconX + sizeXform + 5, yRow2+ yStep, sizeXform*0.6, sizeYform, hwnd, (HMENU)ID_EDIT_W_PERCENT, hInstance, NULL);

        CreateWindowA("STATIC", "%", WS_CHILD | WS_VISIBLE,
            xCol2 + 130, yRow2+yStepTxt, 12, 18, hwnd, NULL, hInstance, NULL);

        CreateWindowA("STATIC", "H", WS_CHILD | WS_VISIBLE,
            xCol2, yRow2 + yStep + yStepTxt, 20, 18, hwnd, NULL, hInstance, NULL);

        g_ui.editH = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL | ES_RIGHT,
            xCol2 + skipIconX, yRow2+yStep*2, sizeXform, sizeYform, hwnd, (HMENU)ID_EDIT_H, hInstance, NULL);

        g_ui.editHPercent = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL | ES_RIGHT,
            xCol2 + skipIconX + sizeXform + 5, yRow2+yStep*2, sizeXform*0.6, sizeYform, hwnd, (HMENU)ID_EDIT_H_PERCENT, hInstance, NULL);

        CreateWindowA("STATIC", "%", WS_CHILD | WS_VISIBLE,
            xCol2 + 130, yRow2+yStep+yStepTxt, 12, 18, hwnd, NULL, hInstance, NULL);

        // COLOR
        CreateSectionIcon(hwnd, hInstance, xCol2, yRow1, IDI_ICON_COLOR);
        CreateWindowA("STATIC", "Color", WS_CHILD | WS_VISIBLE,
            xCol2 + skipIconX, yRow1, 120, 18, hwnd, NULL, hInstance, NULL);

        g_ui.preview = CreateWindowExA(
            0, PREVIEW_CLASS, "",
            WS_CHILD | WS_VISIBLE,
            xCol2, yRow1 + yStep, screenComboW, 30,
            hwnd, (HMENU)ID_PREVIEW_COLOR, hInstance, NULL
        );

        CreateSectionIcon(hwnd, hInstance, xCol2, yRow1 + yStep*2+8, IDI_ICON_OPACITE);
        CreateWindowA("STATIC", "Opacity", WS_CHILD | WS_VISIBLE,
            xCol2 + skipIconX, yRow1 + yStep*2 + 8, 120, 18, hwnd, NULL, hInstance, NULL);

        g_ui.valueOpacity = CreateWindowA("STATIC", "100%", WS_CHILD | WS_VISIBLE,
            xCol2 + 110, yRow1 + yStep*2+8, 46, 18, hwnd, (HMENU)ID_VALUE_OPACITY, hInstance, NULL);

        g_ui.sliderOpacity = CreateWindowExA(0, TRACKBAR_CLASSA, "",
            WS_CHILD | WS_VISIBLE,
            xCol2, yRow1 + yStep*3, screenComboW, 28, hwnd, (HMENU)ID_SLIDER_OPACITY, hInstance, NULL);

        SendMessage(g_ui.sliderOpacity, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));

        // IMAGE
        CreateSectionIcon(hwnd, hInstance, xCol3, yRow1, IDI_ICON_IMAGE);
        CreateWindowA("STATIC", "Image", WS_CHILD | WS_VISIBLE,
            xCol3 + skipIconX, yRow1, 120, 18, hwnd, NULL, hInstance, NULL);

        g_ui.btnBrowseImage = CreateWindowA("BUTTON", "Browse...",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            xCol3, yRow1 + yStep, screenComboW, 24,
            hwnd, (HMENU)ID_BTN_BROWSE_IMAGE, hInstance, NULL);

        g_ui.btnClearImage = CreateWindowA("BUTTON", "Clear image",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            xCol3, yRow1 + yStep * 2, screenComboW, 24,
            hwnd, (HMENU)ID_BTN_CLEAR_IMAGE, hInstance, NULL);

        g_ui.btnFitImage = CreateWindowA("BUTTON", "Fit to image",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            xCol3, yRow1 + yStep * 3, screenComboW, 24,
            hwnd, (HMENU)ID_BTN_FIT_IMAGE, hInstance, NULL);
        
        g_ui.checkKeepImageRatio = CreateWindowA(
            "BUTTON", "Keep aspect ratio",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            xCol3, yRow1 + yStep * 4, screenComboW, 22,
            hwnd, (HMENU)ID_CHECK_KEEP_RATIO, hInstance, NULL
        );

        g_ui.labelImagePath = CreateWindowA("STATIC", "No image",
            WS_CHILD | WS_VISIBLE,
            xCol3, yRow1 + yStep * 5 + 4, screenComboW, 18,
            hwnd, NULL, hInstance, NULL);

        // MAIN ACTIONS
        CreateWindowA("BUTTON", "Reset",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            xCol1+10, 270, 130, 30,
            hwnd, (HMENU)ID_BTN_RESET, hInstance, NULL);
        
        CreateWindowA(
            "STATIC",
            "made with",
            WS_CHILD | WS_VISIBLE,
            xCol2 + 5, 280, 100, 20,
            hwnd, NULL, hInstance, NULL
        );
        CreateSectionIcon(hwnd, hInstance, xCol2 + 70, 280, IDI_ICON_LOVE);
        CreateWindowA(
            "STATIC",
            "by Kodp",
            WS_CHILD | WS_VISIBLE,
            xCol2 + 90, 280, 100, 20,
            hwnd, NULL, hInstance, NULL
        );

        CreateWindowA("BUTTON", "Quit",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            xCol3+10, 270, 130, 30,
            hwnd, (HMENU)ID_BTN_QUIT, hInstance, NULL);

        ApplyFontToChildren(hwnd);

        g_monitors.Refresh();
        if (g_monitors.Count() <= 0)
            g_app.monitorIndex = 0;
        else
            g_app.monitorIndex = ClampInt(g_app.monitorIndex, 0, g_monitors.Count() - 1);

        RebindPositionRanges();
        SyncUiFromState();
        RefreshOverlayNow();
        return 0;
    }

    case WM_HSCROLL:
    {
        HWND src = (HWND)lParam;
        if (src == g_ui.sliderOpacity)
        {
            ApplyAllLive(0);
            return 0;
        }
        break;
    }

    case WM_COMMAND:
    {
        const int id = LOWORD(wParam);
        const int code = HIWORD(wParam);

        if (id == ID_BTN_QUIT)
        {
            DestroyWindow(hwnd);
            return 0;
        }

        if (id == ID_BTN_RESET)
        {
            ResetOverlay();
            return 0;
        }

        if (id == ID_COMBO_MONITOR && code == CBN_SELCHANGE)
        {
            ApplyAllLive(0);
            return 0;
        }

        if ((id == ID_EDIT_X || id == ID_EDIT_Y ||
            id == ID_EDIT_W || id == ID_EDIT_H ||
            id == ID_EDIT_W_PERCENT || id == ID_EDIT_H_PERCENT) &&
            code == EN_CHANGE)
        {
            if (g_isSyncingUi)
                return 0;

            ApplyAllLive(id);
            return 0;
        }

        if (id == ID_BTN_CORNER_TL || id == ID_BTN_CORNER_TR ||
            id == ID_BTN_CORNER_BL || id == ID_BTN_CORNER_BR)
        {
            MoveOverlayToCorner(id);
            return 0;
        }

        if (id == ID_BTN_BROWSE_IMAGE)
        {
            BrowseImageFile(hwnd);
            return 0;
        }

        if (id == ID_BTN_CLEAR_IMAGE)
        {
            ClearImageSelection();
            return 0;
        }

        if (id == ID_BTN_FIT_IMAGE)
        {
            FitOverlayToImage(hwnd);
            return 0;
        }

        if (id == ID_CHECK_KEEP_RATIO)
        {
            g_app.keepImageAspectRatio =
                (SendMessage(g_ui.checkKeepImageRatio, BM_GETCHECK, 0, 0) == BST_CHECKED);

            if (g_app.keepImageAspectRatio && g_app.useImage && g_app.image)
            {
                int screenW = 0;
                int screenH = 0;
                if (GetCurrentMonitorSize(screenW, screenH))
                {
                    const int imgW = (int)g_app.image->GetWidth();
                    const int imgH = (int)g_app.image->GetHeight();

                    ApplyImageAspectFromHeightBounded(
                        imgW, imgH,
                        screenW, screenH,
                        g_app.overlayW, g_app.overlayH
                    );

                    g_app.overlayX = ClampInt(g_app.overlayX, 0, screenW - g_app.overlayW);
                    g_app.overlayY = ClampInt(g_app.overlayY, 0, screenH - g_app.overlayH);
                }
            }

            SyncUiFromState();
            RefreshOverlayNow();
            return 0;
        }

        break;
    }

    case WM_DESTROY:
    {
        if (g_ui.iconCornerTL) { DestroyIcon(g_ui.iconCornerTL); g_ui.iconCornerTL = NULL; }
        if (g_ui.iconCornerTR) { DestroyIcon(g_ui.iconCornerTR); g_ui.iconCornerTR = NULL; }
        if (g_ui.iconCornerBL) { DestroyIcon(g_ui.iconCornerBL); g_ui.iconCornerBL = NULL; }
        if (g_ui.iconCornerBR) { DestroyIcon(g_ui.iconCornerBR); g_ui.iconCornerBR = NULL; }
        if (g_app.overlayHwnd)
        {
            DestroyWindow(g_app.overlayHwnd);
            g_app.overlayHwnd = NULL;
        }

        if (g_app.font)
        {
            DeleteObject(g_app.font);
            g_app.font = NULL;
        }

        if (g_app.bgBrush)
        {
            DeleteObject(g_app.bgBrush);
            g_app.bgBrush = NULL;
        }

        if (g_app.panelBrush)
        {
            DeleteObject(g_app.panelBrush);
            g_app.panelBrush = NULL;
        }

        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

bool RegisterControlWindowClass(HINSTANCE hInstance)
{
    WNDCLASSA wcPreview{};
    wcPreview.lpfnWndProc = PreviewProc;
    wcPreview.hInstance = hInstance;
    wcPreview.lpszClassName = PREVIEW_CLASS;
    wcPreview.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcPreview.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (!RegisterClassA(&wcPreview))
        return false;

    HICON hIconBig = (HICON)LoadImage(
        hInstance,
        MAKEINTRESOURCE(IDI_APP_ICON),
        IMAGE_ICON,
        32, 32,
        LR_DEFAULTCOLOR
    );

    HICON hIconSmall = (HICON)LoadImage(
        hInstance,
        MAKEINTRESOURCE(IDI_APP_ICON),
        IMAGE_ICON,
        24, 24,
        LR_DEFAULTCOLOR
    );

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ControlProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CONTROL_CLASS;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_app.bgBrush;
    wc.hIcon = hIconBig;
    wc.hIconSm = hIconSmall;

    return RegisterClassExA(&wc) != 0;
}

HWND CreateControlWindow(HINSTANCE hInstance, int nCmdShow)
{
    HWND hwnd = CreateWindowExA(
        0,
        CONTROL_CLASS,
        "Simple Overlay",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        400, 400, 560, 350,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd)
    {
        SetWindowPos(
            hwnd,
            HWND_TOP, // HWND_TOPMOST
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
        );

        HICON hIconBig = (HICON)LoadImage(
            hInstance,
            MAKEINTRESOURCE(IDI_APP_ICON),
            IMAGE_ICON,
            32, 32,
            LR_DEFAULTCOLOR
        );
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);

        HICON hIconSmall = (HICON)LoadImage(
            hInstance,
            MAKEINTRESOURCE(IDI_APP_ICON),
            IMAGE_ICON,
            24, 24,
            LR_DEFAULTCOLOR
        );
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);

        BOOL dark = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

        COLORREF captionColor = COLOR_BG;
        DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor));

        COLORREF textColor = COLOR_TEXT;
        DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &textColor, sizeof(textColor));

        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);
    }

    return hwnd;
}