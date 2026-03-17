#pragma once
#include <windows.h>

bool RegisterControlWindowClass(HINSTANCE hInstance);
HWND CreateControlWindow(HINSTANCE hInstance, int nCmdShow);