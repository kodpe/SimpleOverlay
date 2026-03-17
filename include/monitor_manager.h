#pragma once
#include <windows.h>
#include <vector>
#include <string>

struct MonitorInfo
{
    RECT rcMonitor{};
    bool primary = false;
    std::string displayName;
};

class MonitorManager
{
public:
    bool Refresh();
    int Count() const;
    int GetPrimaryIndex() const;
    const MonitorInfo* Get(int index) const;
    const std::vector<MonitorInfo>& GetAll() const;

private:
    std::vector<MonitorInfo> monitors;
};

extern MonitorManager g_monitors;