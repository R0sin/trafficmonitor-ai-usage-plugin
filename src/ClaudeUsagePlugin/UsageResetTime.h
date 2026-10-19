#pragma once

#include <windows.h>
#include <string>

namespace UsageResetTime
{
inline bool UnixSecondsToLocalText(long long unix_seconds, std::wstring& text)
{
    if (unix_seconds < 0)
        return false;

    FILETIME file_time{};
    ULARGE_INTEGER value{};
    value.QuadPart = (static_cast<unsigned long long>(unix_seconds) + 11644473600ULL) * 10000000ULL;
    file_time.dwLowDateTime = value.LowPart;
    file_time.dwHighDateTime = value.HighPart;

    SYSTEMTIME utc_time{};
    if (!FileTimeToSystemTime(&file_time, &utc_time))
        return false;

    SYSTEMTIME local_time{};
    if (!SystemTimeToTzSpecificLocalTime(nullptr, &utc_time, &local_time))
        return false;

    const int date_length = GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &local_time, nullptr, nullptr, 0, nullptr);
    if (date_length <= 1)
        return false;

    const int time_length = GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &local_time, nullptr, nullptr, 0);
    if (time_length <= 1)
        return false;

    std::wstring date_text(static_cast<size_t>(date_length - 1), L'\0');
    std::wstring time_text(static_cast<size_t>(time_length - 1), L'\0');
    if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &local_time, nullptr, &date_text[0], date_length, nullptr) == 0)
        return false;
    if (GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &local_time, nullptr, &time_text[0], time_length) == 0)
        return false;

    text = date_text;
    text += L" ";
    text += time_text;
    return true;
}

inline std::wstring FormatDurationFromSeconds(unsigned long long total_seconds)
{
    if (total_seconds < 60ULL)
        return L"<1m";

    const unsigned long long total_minutes = total_seconds / 60ULL;
    const unsigned long long days = total_minutes / (24ULL * 60ULL);
    const unsigned long long hours = (total_minutes / 60ULL) % 24ULL;
    const unsigned long long minutes = total_minutes % 60ULL;

    std::wstring text;
    if (days > 0)
    {
        text = std::to_wstring(days) + L"d";
        if (hours > 0)
            text += L" " + std::to_wstring(hours) + L"h";
        return text;
    }

    if (hours > 0)
    {
        text = std::to_wstring(hours) + L"h";
        if (minutes > 0)
            text += L" " + std::to_wstring(minutes) + L"m";
        return text;
    }

    return std::to_wstring(minutes) + L"m";
}

inline std::wstring FormatResetRemaining(long long reset_at_unix_seconds)
{
    FILETIME now_file_time{};
    GetSystemTimeAsFileTime(&now_file_time);

    ULARGE_INTEGER now_value{};
    now_value.LowPart = now_file_time.dwLowDateTime;
    now_value.HighPart = now_file_time.dwHighDateTime;
    if (now_value.QuadPart < 116444736000000000ULL)
        return std::wstring();

    const long long now_unix_seconds = static_cast<long long>((now_value.QuadPart - 116444736000000000ULL) / 10000000ULL);
    if (reset_at_unix_seconds <= now_unix_seconds)
        return L"now";

    return L"in " + FormatDurationFromSeconds(static_cast<unsigned long long>(reset_at_unix_seconds - now_unix_seconds));
}

}
