#include "monitor_manager.h"
#include <cstdio>

MonitorManager g_monitors;

static BOOL CALLBACK EnumMonProc(HMONITOR hMon, HDC, LPRECT, LPARAM lParam)
{
    auto* vec = reinterpret_cast<std::vector<MonitorInfo>*>(lParam);

    MONITORINFOEXA mi{};
    mi.cbSize = sizeof(mi);

    if (!GetMonitorInfoA(hMon, &mi))
        return TRUE;

    MonitorInfo info{};
    info.rcMonitor = mi.rcMonitor;
    info.primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;

    int w = mi.rcMonitor.right - mi.rcMonitor.left;
    int h = mi.rcMonitor.bottom - mi.rcMonitor.top;

    char buf[256];
    std::snprintf(
        buf, sizeof(buf),
        "%s - %dx%d%s",
        mi.szDevice,
        w, h,
        info.primary ? " (principal)" : ""
    );

    info.displayName = buf;
    vec->push_back(info);
    return TRUE;
}

bool MonitorManager::Refresh()
{
    monitors.clear();
    EnumDisplayMonitors(NULL, NULL, EnumMonProc, reinterpret_cast<LPARAM>(&monitors));
    return !monitors.empty();
}

int MonitorManager::Count() const
{
    return (int)monitors.size();
}

int MonitorManager::GetPrimaryIndex() const
{
    for (int i = 0; i < (int)monitors.size(); ++i)
    {
        if (monitors[i].primary)
            return i;
    }
    return monitors.empty() ? -1 : 0;
}

const MonitorInfo* MonitorManager::Get(int index) const
{
    if (index < 0 || index >= (int)monitors.size())
        return nullptr;
    return &monitors[index];
}

const std::vector<MonitorInfo>& MonitorManager::GetAll() const
{
    return monitors;
}