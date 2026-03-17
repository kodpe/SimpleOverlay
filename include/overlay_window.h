#pragma once
#include <windows.h>

bool RegisterOverlayWindowClass(HINSTANCE hInstance);
HWND CreateOverlayWindow(HINSTANCE hInstance);

void ApplyOverlayVisuals();
void ApplyOverlayPlacement();
void EnsureControlAboveOverlay();
void RefreshOverlayNow();