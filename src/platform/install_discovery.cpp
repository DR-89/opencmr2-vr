// PORT: read-only Windows installation discovery. Kept separate from the game
// compatibility headers, which replace several native Windows types.
#include "platform/install_discovery.h"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <filesystem>
#include <set>
#include <cwctype>

namespace {
namespace fs = std::filesystem;
struct Key {
    HKEY handle = nullptr;
    Key(HKEY parent, const wchar_t *name, REGSAM view) {
        RegOpenKeyExW(parent, name, 0, KEY_READ | view, &handle);
    }
    ~Key() { if (handle) RegCloseKey(handle); }
};

std::wstring ReadString(HKEY key, const wchar_t *name) {
    if (!key) return {};
    DWORD type = 0, size = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS ||
        (type != REG_SZ && type != REG_EXPAND_SZ) || size > 65536 || size % sizeof(wchar_t)) return {};
    std::vector<wchar_t> data(size / sizeof(wchar_t) + 1, 0);
    if (RegQueryValueExW(key, name, nullptr, &type, (BYTE *)data.data(), &size) != ERROR_SUCCESS) return {};
    std::wstring value(data.data());
    if (type == REG_EXPAND_SZ) {
        DWORD length = ExpandEnvironmentStringsW(value.c_str(), nullptr, 0);
        if (!length || length > 32768) return {};
        std::vector<wchar_t> expanded(length);
        if (!ExpandEnvironmentStringsW(value.c_str(), expanded.data(), length)) return {};
        value = expanded.data();
    }
    return value;
}

bool IsGameName(const std::wstring &name) {
    std::wstring normalized;
    // Keep the exact game number: CMR 2005 and later games are not candidates.
    for (wchar_t c : name) if (std::iswalnum(c)) normalized += (wchar_t)std::towlower(c);
    return normalized == L"colinmcraerally2" || normalized == L"colinmcraerally20" ||
           normalized == L"cmr2" || normalized == L"cmr20";
}

void Add(std::set<std::wstring> &paths, std::wstring path) {
    size_t first = path.find_first_not_of(L" \t\r\n\"");
    size_t last = path.find_last_not_of(L" \t\r\n\"");
    if (first == std::wstring::npos) return;
    path = path.substr(first, last - first + 1);
    // Do not probe disconnected shares, mapped network drives or optical media
    // during startup. The folder picker still supports explicitly chosen paths.
    if (path.size() < 3 || path[1] != L':' || (path[2] != L'\\' && path[2] != L'/')) return;
    std::wstring drive = path.substr(0, 2) + L"\\";
    if (GetDriveTypeW(drive.c_str()) != DRIVE_FIXED) return;
    paths.insert(fs::path(path).lexically_normal().wstring());
}

void ReadLocations(std::set<std::wstring> &paths, HKEY key) {
    for (const wchar_t *value : {L"Game_HDPath", L"InstallLocation", L"InstallPath", L"InstallDir", L"Path"})
        Add(paths, ReadString(key, value));
}

void ReadRegistry(std::set<std::wstring> &paths, HKEY hive, REGSAM view) {
    for (const wchar_t *vendor : {L"SOFTWARE\\Codemasters", L"SOFTWARE\\Codemasters Software"}) {
        Key parent(hive, vendor, view);
        if (!parent.handle) continue;
        for (DWORD i = 0; i < 256; ++i) {
            wchar_t name[256]; DWORD length = 256;
            LONG result = RegEnumKeyExW(parent.handle, i, name, &length, nullptr, nullptr, nullptr, nullptr);
            if (result == ERROR_NO_MORE_ITEMS) break;
            if (result != ERROR_SUCCESS || !IsGameName(name)) continue;
            Key game(parent.handle, name, view);
            ReadLocations(paths, game.handle);
        }
    }
    Key uninstall(hive, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", view);
    if (!uninstall.handle) return;
    for (DWORD i = 0; i < 4096; ++i) {
        wchar_t name[256]; DWORD length = 256;
        LONG result = RegEnumKeyExW(uninstall.handle, i, name, &length, nullptr, nullptr, nullptr, nullptr);
        if (result == ERROR_NO_MORE_ITEMS) break;
        if (result != ERROR_SUCCESS) continue;
        Key game(uninstall.handle, name, view);
        if (IsGameName(ReadString(game.handle, L"DisplayName"))) ReadLocations(paths, game.handle);
    }
}

void AddNamedGames(std::set<std::wstring> &paths, const fs::path &parent) {
    for (const wchar_t *name : {L"Colin McRae Rally 2", L"Colin McRae Rally 2.0", L"CMR2"})
        Add(paths, (parent / name).wstring());
}
}
#endif

std::vector<std::string> FindInstalledGameCandidates() {
    std::vector<std::string> result;
#ifdef _WIN32
    std::set<std::wstring> paths;
    for (HKEY hive : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE})
        for (REGSAM view : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) ReadRegistry(paths, hive, view);
    for (const wchar_t *variable : {L"ProgramFiles", L"ProgramFiles(x86)", L"ProgramW6432"}) {
        wchar_t value[32768];
        DWORD length = GetEnvironmentVariableW(variable, value, 32768);
        if (!length || length >= 32768) continue;
        AddNamedGames(paths, fs::path(value));
        AddNamedGames(paths, fs::path(value) / L"Codemasters");
    }
    wchar_t drives[512];
    DWORD length = GetLogicalDriveStringsW(512, drives);
    if (length && length < 512) for (const wchar_t *drive = drives; *drive; drive += wcslen(drive) + 1) {
        if (GetDriveTypeW(drive) != DRIVE_FIXED) continue;
        for (const wchar_t *folder : {L"", L"Games", L"GOG Games", L"Codemasters", L"Games\\Codemasters"})
            AddNamedGames(paths, fs::path(drive) / folder);
    }
    for (const auto &path : paths) result.push_back(fs::path(path).generic_u8string());
#endif
    return result;
}
