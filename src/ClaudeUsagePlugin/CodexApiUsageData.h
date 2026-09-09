#pragma once

#include <array>
#include <mutex>
#include <string>

class CCodexApiUsageData
{
public:
    struct Metric
    {
        bool available{};
        double percentage{};
        std::wstring text{ L"--" };
    };
    static CCodexApiUsageData& Instance();
    ~CCodexApiUsageData();
    void RefreshIfNeeded();
    std::wstring GetValueText(int index) const;
    Metric GetMetric(int index) const;
    std::wstring GetTooltipText() const;

private:
    mutable std::mutex m_mutex;
    std::array<Metric, 4> m_metrics;
    std::wstring m_tooltip{ L"Codex API usage unavailable" };
    std::wstring m_fingerprint;
    unsigned long long m_last_read{};
    unsigned long long m_last_launch{};
    unsigned long long m_changed_tick{};
    bool m_helper_failed{};
    void* m_process{};
};

#define g_codex_api_usage_data CCodexApiUsageData::Instance()
