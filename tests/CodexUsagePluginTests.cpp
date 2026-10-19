#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "PluginInterface.h"
#include "../src/ClaudeUsagePlugin/CodexApiUsageData.h"
#include <ctime>

namespace
{
using GetPluginInstance = ITMPlugin* (*)();

struct TestCase
{
    const wchar_t* name;
    const char* rate_limits;
    const wchar_t* expected_5h;
    const wchar_t* expected_7d;
};

const TestCase TEST_CASES[] = {
    {
        L"weekly-primary",
        R"({"primary":{"used_percent":30,"window_minutes":10080,"resets_at":1893456000},"secondary":null})",
        L"--",
        L"30%",
    },
    {
        L"both-windows",
        R"({"primary":{"used_percent":12,"window_minutes":300,"resets_at":1893456000},"secondary":{"used_percent":34,"window_minutes":10080,"resets_at":1893456000}})",
        L"12%",
        L"34%",
    },
    {
        L"swapped-windows",
        R"({"primary":{"used_percent":34,"window_minutes":10080,"resets_at":1893456000},"secondary":{"used_percent":12,"window_minutes":300,"resets_at":1893456000}})",
        L"12%",
        L"34%",
    },
    {
        L"legacy-no-window",
        R"({"primary":{"used_percent":12,"resets_at":1893456000},"secondary":{"used_percent":34,"resets_at":1893456000}})",
        L"12%",
        L"34%",
    },
    {
        L"unknown-window",
        R"({"primary":{"used_percent":50,"window_minutes":1440,"resets_at":1893456000},"secondary":null})",
        L"--",
        L"--",
    },
};

const TestCase* FindTestCase(const std::wstring& name)
{
    for (const TestCase& test_case : TEST_CASES)
    {
        if (name == test_case.name)
            return &test_case;
    }
    return nullptr;
}

std::filesystem::path CreateFixture(const TestCase& test_case)
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        (L"trafficmonitor-ai-usage-plugin-test-" + std::to_wstring(GetCurrentProcessId()));
    const std::filesystem::path sessions = root / L"sessions" / L"2026" / L"07" / L"27";
    std::filesystem::create_directories(sessions);

    std::ofstream output(sessions / L"rollout-test.jsonl", std::ios::binary);
    output
        << R"({"timestamp":"2026-07-27T08:12:08Z","type":"event_msg","payload":{"type":"token_count","rate_limits":)"
        << test_case.rate_limits
        << "}}\n";
    output.close();
    return root;
}

bool ExpectValue(IPluginItem* item, const wchar_t* expected, const wchar_t* label)
{
    if (item == nullptr)
    {
        std::wcerr << label << L" item was not available.\n";
        return false;
    }

    const std::wstring actual = item->GetItemValueText();
    if (actual == expected)
        return true;

    std::wcerr << label << L": expected \"" << expected << L"\", got \"" << actual << L"\".\n";
    return false;
}
}

int wmain(int argc, wchar_t* argv[])
{
    if ((argc == 2 && std::wstring(argv[1]) == L"api-snapshot")
        || (argc == 3 && std::wstring(argv[2]) == L"api-bars"))
    {
        const auto root = std::filesystem::temp_directory_path() /
            (L"codex-api-snapshot-test-" + std::to_wstring(GetCurrentProcessId()));
        const auto cache_dir = root / L"trafficmonitor-claude-usage-plugin";
        std::filesystem::create_directories(cache_dir);
        std::ofstream(root / L"auth.json") << "{}";
        std::ofstream(root / L"config.toml") << "";
        SetEnvironmentVariableW(L"CODEX_HOME", root.c_str());
        SetEnvironmentVariableW(L"LOCALAPPDATA", root.c_str());
        const auto cache = cache_dir / L"codex-api-usage.ini";
        const auto write = [&](const wchar_t* key, const std::wstring& value) {
            WritePrivateProfileStringW(L"usage", key, value.c_str(), cache.c_str());
        };
        write(L"version", L"2");
        write(L"fingerprint", L"44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a");
        write(L"updated", std::to_wstring(std::time(nullptr)));
        write(L"status", L"OK");
        write(L"five_hour_percentage", L"12.5");
        write(L"five_hour_detail", L"2.50/20.00 USD");
        write(L"five_hour_reset_at", std::to_wstring(std::time(nullptr) + 7200));
        write(L"seven_day_reset_at", L"1");
        write(L"one_day_percentage", L"0.0");
        write(L"seven_day_percentage", L"100.0");
        write(L"total_percentage", L"25.0");
        if (argc == 3)
        {
            HMODULE module = LoadLibraryW(argv[1]);
            const auto get_instance = module ? reinterpret_cast<GetPluginInstance>(GetProcAddress(module, "TMPluginGetInstance")) : nullptr;
            if (!get_instance) return 2;
            ITMPlugin* plugin = get_instance();
            plugin->DataRequired();
            Sleep(2100);
            plugin->DataRequired();
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = 800;
            info.bmiHeader.biHeight = -64;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            void* pixels{};
            HDC dc = CreateCompatibleDC(nullptr);
            HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
            const auto old_bitmap = SelectObject(dc, bitmap);
            HFONT font = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            const auto old_font = SelectObject(dc, font);
            const wchar_t* expected[] = { L"12.5%", L"0%", L"100%", L"25%" };
            const wchar_t* ids[] = { L"CodexApi5Hours", L"CodexApi1Day", L"CodexApi7Days", L"CodexApiTotal" };
            bool passed = plugin->GetItem(8) == nullptr;
            for (int theme = 0; theme < 2; ++theme)
            {
                RECT row{ 0, theme * 32, 800, (theme + 1) * 32 };
                HBRUSH background = CreateSolidBrush(theme ? RGB(24, 24, 24) : RGB(255, 255, 255));
                FillRect(dc, &row, background);
                DeleteObject(background);
                for (int index = 0; index < 4; ++index)
                {
                    auto* item = plugin->GetItem(4 + index);
                    passed = passed && ExpectValue(item, expected[index], ids[index]);
                    if (!item) continue;
                    passed = passed && item->IsCustomDraw() && std::wstring(item->GetItemId()) == ids[index]
                        && item->GetItemWidthEx(dc) <= 200;
                    item->DrawItem(dc, index * 200, theme * 32, 200, 32, theme != 0);
                    passed = passed && GetPixel(dc, index * 200 + 5, theme * 32 + 5)
                        != (theme ? RGB(24, 24, 24) : RGB(255, 255, 255));
                }
            }
            GdiFlush();
            BITMAPFILEHEADER header{};
            header.bfType = 0x4d42;
            header.bfOffBits = sizeof(header) + sizeof(info.bmiHeader);
            header.bfSize = header.bfOffBits + 800 * 64 * 4;
            const auto image_path = std::filesystem::path(argv[1]).parent_path() / L"api-bars-test.bmp";
            std::ofstream image(image_path, std::ios::binary);
            image.write(reinterpret_cast<const char*>(&header), sizeof(header));
            image.write(reinterpret_cast<const char*>(&info.bmiHeader), sizeof(info.bmiHeader));
            image.write(static_cast<const char*>(pixels), 800 * 64 * 4);
            SelectObject(dc, old_font);
            SelectObject(dc, old_bitmap);
            DeleteObject(font);
            DeleteObject(bitmap);
            DeleteDC(dc);
            std::filesystem::remove_all(root);
            std::wcout << (passed ? L"PASS" : L"FAIL") << L" api-bars\n";
            return passed ? 0 : 1;
        }
        auto& data = CCodexApiUsageData::Instance();
        data.RefreshIfNeeded();
        Sleep(2100);
        data.RefreshIfNeeded();
        bool passed = data.GetValueText(0) == L"12.5%" && data.GetValueText(1) == L"0%"
            && data.GetValueText(2) == L"100%" && data.GetValueText(3) == L"25%"
            && data.GetMetric(0).available && data.GetMetric(0).percentage == 12.5
            && data.GetTooltipText().find(L"(resets in 1h 59m at ") != std::wstring::npos
            && data.GetTooltipText().find(L"(resets now at ") != std::wstring::npos
            && data.GetTooltipText().find(L"5h: 13% (resets ") != std::wstring::npos
            && data.GetTooltipText().find(L"Updated") == std::wstring::npos
            && data.GetTooltipText().find(L")\n2.50/20.00 USD\n") != std::wstring::npos
            && data.GetTooltipText().find(L"used") == std::wstring::npos
            && data.GetTooltipText().find(L"1d: 0%\n") != std::wstring::npos;
        write(L"five_hour_percentage", L"NaN");
        write(L"one_day_percentage", L"101");
        write(L"seven_day_percentage", L"12oops");
        write(L"total_percentage", L"");
        Sleep(1100);
        data.RefreshIfNeeded();
        for (int i = 0; i < 4; ++i)
            passed = passed && !data.GetMetric(i).available && data.GetValueText(i) == L"--";
        write(L"five_hour_percentage", L"12.5");
        write(L"updated", std::to_wstring(std::time(nullptr) - 181));
        Sleep(1100);
        data.RefreshIfNeeded();
        passed = passed && data.GetValueText(0) == L"--";
        write(L"updated", std::to_wstring(std::time(nullptr)));
        write(L"status", L"Usage HTTP 401");
        Sleep(1100);
        data.RefreshIfNeeded();
        passed = passed && data.GetValueText(0) == L"--"
            && data.GetTooltipText().find(L"401") != std::wstring::npos;
        write(L"status", L"OK");
        std::ofstream(root / L"auth.json") << "{\"account\":\"different\"}";
        Sleep(1100);
        data.RefreshIfNeeded();
        passed = passed && data.GetValueText(0) == L"--";
        std::filesystem::remove_all(root);
        std::wcout << (passed ? L"PASS" : L"FAIL") << L" api-snapshot\n";
        return passed ? 0 : 1;
    }
    if (argc != 3)
    {
        std::wcerr << L"Usage: CodexUsagePluginTests.exe <plugin-dll> <scenario>\n";
        return 2;
    }

    const TestCase* test_case = FindTestCase(argv[2]);
    if (test_case == nullptr)
    {
        std::wcerr << L"Unknown scenario: " << argv[2] << L"\n";
        return 2;
    }

    const std::filesystem::path fixture_root = CreateFixture(*test_case);
    if (!SetEnvironmentVariableW(L"CODEX_HOME", fixture_root.c_str()))
    {
        std::wcerr << L"Failed to set CODEX_HOME.\n";
        return 2;
    }

    HMODULE module = LoadLibraryW(argv[1]);
    if (module == nullptr)
    {
        std::wcerr << L"Failed to load plugin DLL: " << GetLastError() << L"\n";
        std::filesystem::remove_all(fixture_root);
        return 2;
    }

    const auto get_instance = reinterpret_cast<GetPluginInstance>(GetProcAddress(module, "TMPluginGetInstance"));
    if (get_instance == nullptr)
    {
        std::wcerr << L"TMPluginGetInstance export was not found.\n";
        std::filesystem::remove_all(fixture_root);
        return 2;
    }

    ITMPlugin* plugin = get_instance();
    plugin->DataRequired();

    const bool passed =
        ExpectValue(plugin->GetItem(2), test_case->expected_5h, L"Codex 5h") &&
        ExpectValue(plugin->GetItem(3), test_case->expected_7d, L"Codex 7d");

    std::filesystem::remove_all(fixture_root);
    if (!passed)
        return 1;

    std::wcout << L"PASS " << test_case->name << L"\n";
    return 0;
}
