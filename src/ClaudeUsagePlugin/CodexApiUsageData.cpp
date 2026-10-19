#include <windows.h>
#include "CodexApiUsageData.h"
#include "UsageResetTime.h"

#include <bcrypt.h>
#include <ctime>
#include <cmath>
#include <cwchar>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace
{
std::wstring Env(const wchar_t* name)
{
    const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (!size) return {};
    std::vector<wchar_t> buffer(size);
    if (!GetEnvironmentVariableW(name, buffer.data(), size)) return {};
    return buffer.data();
}

std::wstring Fingerprint()
{
    auto root = Env(L"CODEX_HOME");
    if (root.empty()) root = Env(L"USERPROFILE") + L"\\.codex";
    if (root.compare(0, 2, L"~\\") == 0 || root.compare(0, 2, L"~/") == 0)
        root = Env(L"USERPROFILE") + root.substr(1);
    std::vector<unsigned char> bytes;
    for (const auto* name : { L"auth.json", L"config.toml" })
    {
        HANDLE file = CreateFileW((root + L"\\" + name).c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (file == INVALID_HANDLE_VALUE) return {};
        LARGE_INTEGER size{};
        const bool valid = GetFileSizeEx(file, &size) && size.QuadPart >= 0 && size.QuadPart <= 1024 * 1024;
        if (!valid) { CloseHandle(file); return {}; }
        const size_t offset = bytes.size();
        bytes.resize(offset + static_cast<size_t>(size.QuadPart));
        DWORD read{};
        const bool ok = ReadFile(file, bytes.data() + offset, static_cast<DWORD>(size.QuadPart), &read, nullptr)
            && read == size.QuadPart;
        CloseHandle(file);
        if (!ok) return {};
    }
    BCRYPT_ALG_HANDLE algorithm{};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return {};
    unsigned char digest[32]{};
    const auto status = BCryptHash(algorithm, nullptr, 0, bytes.data(), static_cast<ULONG>(bytes.size()), digest, sizeof(digest));
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status != 0) return {};
    std::wstring result;
    for (auto byte : digest)
    {
        result += L"0123456789abcdef"[byte >> 4];
        result += L"0123456789abcdef"[byte & 15];
    }
    return result;
}

std::wstring Field(const std::wstring& path, const wchar_t* key, const wchar_t* fallback = L"")
{
    wchar_t value[256]{};
    GetPrivateProfileStringW(L"usage", key, fallback, value, _countof(value), path.c_str());
    return value;
}

HANDLE LaunchHelper()
{
    HMODULE module{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&LaunchHelper), &module)) return nullptr;
    wchar_t path[32768]{};
    if (!GetModuleFileNameW(module, path, _countof(path))) return nullptr;
    const std::wstring module_path(path);
    const auto directory = module_path.substr(0, module_path.find_last_of(L"\\/"));
    const auto script = directory + L"\\ClaudeUsagePlugin\\codex-api-helper.ps1";
    if (GetFileAttributesW(script.c_str()) == INVALID_FILE_ATTRIBUTES) return nullptr;
    const auto powershell = Env(L"SystemRoot") + L"\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
    std::wstring command = L"\"" + powershell + L"\" -NoProfile -ExecutionPolicy Bypass -File \"" + script + L"\"";
    std::vector<wchar_t> buffer(command.begin(), command.end());
    buffer.push_back(0);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(powershell.c_str(), buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
        nullptr, directory.c_str(), &startup, &process)) return nullptr;
    CloseHandle(process.hThread);
    return process.hProcess;
}
}

CCodexApiUsageData& CCodexApiUsageData::Instance()
{
    static CCodexApiUsageData instance;
    return instance;
}

void CCodexApiUsageData::RefreshIfNeeded()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto now = GetTickCount64();
    if (m_last_read && now - m_last_read < 1000) return;
    m_last_read = now;
    m_metrics = {};
    m_tooltip = L"Codex API usage unavailable";
    const auto fingerprint = Fingerprint();
    const bool changed = fingerprint != m_fingerprint;
    m_fingerprint = fingerprint;
    if (changed)
    {
        m_last_launch = 0;
        m_changed_tick = now;
    }
    if (m_process && WaitForSingleObject(m_process, 0) == WAIT_OBJECT_0)
    {
        DWORD exit_code{};
        m_helper_failed = !GetExitCodeProcess(m_process, &exit_code) || exit_code != 0;
        CloseHandle(m_process);
        m_process = nullptr;
    }
    if (fingerprint.empty()) return;
    // Cockpit writes auth.json and config.toml separately. Wait for a stable pair.
    if (now - m_changed_tick < 2000) return;
    const auto cache = Env(L"LOCALAPPDATA") + L"\\trafficmonitor-claude-usage-plugin\\codex-api-usage.ini";
    if (Field(cache, L"version") == L"2" && Field(cache, L"fingerprint") == fingerprint)
    {
        const auto updated = _wtoi64(Field(cache, L"updated").c_str());
        const auto age = static_cast<long long>(std::time(nullptr)) - updated;
        if (updated > 0 && age >= 0 && age <= 180)
        {
            const auto status = Field(cache, L"status");
            m_tooltip += L": " + status;
            if (status == L"OK")
            {
                const wchar_t* prefixes[] = { L"five_hour", L"one_day", L"seven_day", L"total" };
                const wchar_t* labels[] = { L"5h", L"1d", L"7d", L"API key quota" };
                m_tooltip = L"Codex API limits (Sub2API)";
                for (size_t index = 0; index < m_metrics.size(); ++index)
                {
                    auto& metric = m_metrics[index];
                    const auto value = Field(cache, (std::wstring(prefixes[index]) + L"_percentage").c_str());
                    wchar_t* end{};
                    const double percentage = wcstod(value.c_str(), &end);
                    if (!value.empty() && end != value.c_str() && *end == 0 && std::isfinite(percentage)
                        && percentage >= 0 && percentage <= 100)
                    {
                        metric.available = true;
                        metric.percentage = percentage;
                        wchar_t text[32]{};
                        swprintf_s(text, percentage == std::floor(percentage) ? L"%.0f%%" : L"%.1f%%", percentage);
                        metric.text = text;
                    }
                    m_tooltip += L"\n" + std::wstring(labels[index]) + L": "
                        + (metric.available ? std::to_wstring(std::lround(metric.percentage)) + L"%" : metric.text);
                    if (metric.available)
                    {
                        const auto reset_value = Field(cache, (std::wstring(prefixes[index]) + L"_reset_at").c_str());
                        wchar_t* reset_end{};
                        const auto reset_at = _wcstoi64(reset_value.c_str(), &reset_end, 10);
                        std::wstring reset_text;
                        if (!reset_value.empty() && *reset_end == 0 && reset_at > 0 && reset_at <= 253402300799LL
                            && UsageResetTime::UnixSecondsToLocalText(reset_at, reset_text))
                            m_tooltip += L" (resets " + UsageResetTime::FormatResetRemaining(reset_at) + L" at " + reset_text + L")";
                        const auto detail = Field(cache, (std::wstring(prefixes[index]) + L"_detail").c_str());
                        if (!detail.empty()) m_tooltip += L"\n" + detail;
                    }
                    else
                        m_tooltip += L" (limit unavailable)";
                }
            }
        }
        else m_tooltip += L": snapshot expired";
    }
    if (m_helper_failed)
    {
        m_metrics = {};
        m_tooltip = L"Codex API helper failed; check Node.js 22+ on PATH, bundled dependencies, and network access";
    }
    if (!m_process && (!m_last_launch || now - m_last_launch >= 60000))
    {
        m_process = LaunchHelper();
        m_last_launch = now;
        if (!m_process) m_tooltip += L"; bundled helper could not start";
    }
}

CCodexApiUsageData::~CCodexApiUsageData()
{
    if (m_process) CloseHandle(m_process);
}

std::wstring CCodexApiUsageData::GetValueText(int index) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metrics.at(index).text;
}

CCodexApiUsageData::Metric CCodexApiUsageData::GetMetric(int index) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metrics.at(index);
}

std::wstring CCodexApiUsageData::GetTooltipText() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tooltip;
}
