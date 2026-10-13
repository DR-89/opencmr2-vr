// Exercise the actual startup flow with synthetic data and desktop dialogs.
// No original assets, display server or interaction are needed in CI.
#include "platform/platform.h"
#include "platform/testing.h"
#include "port/sys.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
static std::vector<std::string> folders, messages;
static std::vector<std::thread> callbacks;
static int dialogs;
static bool exitSetup, dialogError, exitOnWarning;
static std::vector<std::string> installCandidates;
static int discoveryCalls;
static std::vector<std::string> InstalledCandidates() { ++discoveryCalls; return installCandidates; }

static void Check(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "game data setup: %s (%s)\n", message, SDL_GetError());
        exit(1);
    }
}

static std::string Read(const fs::path &path)
{
    size_t length = 0;
    char *data = (char *)SDL_LoadFile(path.u8string().c_str(), &length);
    Check(data != nullptr, "read fixture");
    std::string result(data, length);
    SDL_free(data);
    return result;
}

static void Write(const fs::path &path, const std::string &data)
{
    fs::create_directories(path.parent_path());
    Check(SDL_SaveFile(path.u8string().c_str(), data.data(), data.size()), "write fixture");
}

static bool MessageBox(const SDL_MessageBoxData *box, int *button)
{
    messages.emplace_back(box->message);
    Check(box->numbuttons == 2, "setup has select and exit buttons");
    *button = exitSetup || (exitOnWarning && box->flags == SDL_MESSAGEBOX_WARNING) ? 0 : 1;
    return true;
}

static void OpenFolder(SDL_DialogFileCallback callback, void *userdata)
{
    ++dialogs;
    Check(!folders.empty() || dialogError, "unexpected folder selector");
    std::string path;
    if (!folders.empty()) {
        path = folders.front();
        folders.erase(folders.begin());
    }
    // Real SDL callbacks can arrive from a worker thread; their path storage
    // expires on return. Delaying also makes startup pump events while waiting.
    callbacks.emplace_back([=] {
        SDL_Delay(20);
        if (dialogError) {
            SDL_SetError("No desktop folder selector available");
            callback(userdata, nullptr, -1);
            return;
        }
        const char *files[] = {path.empty() ? nullptr : path.c_str(), nullptr};
        callback(userdata, files, -1);
    });
}

static bool Start(const fs::path &user, const std::string &explicitData = "")
{
    // SDL_Quit discards its environment snapshot between startup attempts.
    Check(SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true), "dummy video");
    SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), "OPENCMR2_DATA");
    std::string userPath = user.u8string(), data = explicitData;
    char name[] = "game_data_setup", userFlag[] = "--user-dir", dataFlag[] = "--data";
    char *args[] = {name, userFlag, userPath.data(), dataFlag, data.data()};
    bool started = Platform_Init(explicitData.empty() ? 3 : 5, args);
    for (auto &thread : callbacks)
        thread.join();
    callbacks.clear();
    return started;
}

int main()
{
    const fs::path previous = fs::current_path();
    const fs::path root = fs::canonical(fs::temp_directory_path()) /
        ("opencmr2-setup-" + std::to_string(SDL_GetPerformanceCounter()));
    fs::create_directories(root);
    fs::current_path(root);
    const fs::path user = root / fs::u8path(u8"ajustes ñ"), game = root / fs::u8path(u8"juego é con espacios");
    const fs::path ini = user / "opencmr2.ini";
    Write(game / "GAME/Bigfiles/COMMON.BFL", "synthetic main archive");
    Write(game / "frontend/Common.bfl", "synthetic frontend archive");
    Write(game / "Sounds/surface.bfl", "synthetic sound archive");
    Write(root / "incomplete/Game/Bigfiles/common.bfl", "synthetic");
    Write(root / "incomplete/FrontEnd/common.bfl", "");
    fs::create_directories(root / "incomplete/Sounds/surface.bfl");
    // Mixed-case sections, duplicate old values, CRLF, comments and unrelated
    // settings must survive replacing the remembered folder.
    Write(ini, "; keep this comment\r\n[PATHS]\r\ndata = missing\r\n"
               "[video]\r\nfov = 91\r\n[paths]\r\ndata = also-missing\r\n");
    const PlatformTestDialogs mock{OpenFolder, MessageBox};
    Platform_SetTestDialogs(&mock);
    Platform_SetTestInstallCandidates(InstalledCandidates);
    folders = {(root / "incomplete").u8string(), game.u8string()};
    Check(Start(user), "select valid folder after incomplete folder");
    Check(dialogs == 2 && messages.size() == 2, "retry incomplete installation");
    Check(messages.back().find("FrontEnd/common.bfl") != std::string::npos &&
          messages.back().find("Sounds/surface.bfl") != std::string::npos, "list missing archives");
    Check(std::string(Platform_GetDataDir()) == game.u8string() + "/", "copy UTF-8 callback path");
    Check(Platform_GetSettingInt("video.fov", 0) == 91, "preserve unrelated settings");
    SysFile *archive = Sys_OpenFile("Game\\Bigfiles\\common.bfl", SYS_FILE_READ);
    Check(archive != nullptr, "open game archive under selected UTF-8 path");
    char archiveBytes[64]{};
    Check(Sys_ReadFile(archive, archiveBytes, sizeof(archiveBytes)) == 22 &&
          std::string(archiveBytes) == "synthetic main archive", "read selected game files");
    Sys_CloseFile(archive);
    SysFile *save = Sys_OpenFile("gamesave\\setup-test.dat", SYS_FILE_WRITE);
    Check(save != nullptr && Sys_WriteFile(save, "test", 4) == 4, "write saves under UTF-8 user directory");
    Sys_CloseFile(save);
    const std::string saved = Read(ini);
    Check(saved.find("; keep this comment") != std::string::npos &&
          saved.find("data = " + game.u8string()) != std::string::npos &&
          saved.find("also-missing") == std::string::npos, "persist path without stale overrides");
    Platform_Shutdown();

    Check(Start(user), "load remembered folder on next launch");
    Check(dialogs == 2 && Read(ini) == saved, "next launch opens no dialogs or rewrites");
    Check(Platform_GetSettingInt("video.fov", 0) == 91, "reload preserved configuration");
    Platform_Shutdown();

    Check(Start(root / "explicit-user", game.u8string()), "explicit --data skips GUI");
    Check(dialogs == 2, "explicit data needs no selector");
    // Alt+Enter remembers the window mode: the default line changes in place.
    Check(Sys_SetOption("video.fullscreen", 0) && Sys_GetOption("video.fullscreen", 1) == 0 &&
          Sys_SetOption("Extra.Value", 3), "save options");
    const std::string options = Read(root / "explicit-user" / "opencmr2.ini");
    Check(options.find("fullscreen = 1") == std::string::npos &&
          options.find("; 1 = fullscreen") < options.find("fullscreen = 0") &&
          options.find("fullscreen = 0") < options.find("[net]") &&
          options.find("\n[extra]\nvalue = 3\n") != std::string::npos, "options saved in place");
    Platform_Shutdown();
    Check(!Start(root / "invalid-user", (root / "incomplete").u8string()), "invalid explicit data fails");
    Check(!Platform_WasSetupCanceled(), "invalid explicit data is an error");
    Check(dialogs == 2, "invalid --data does not wait for interaction");

    fs::rename(game, root / "moved-game");
    folders = {""};
    Check(!Start(user), "cancel when remembered folder has moved");
    Check(Platform_WasSetupCanceled(), "cancel is a clean exit");
    Check(Read(ini) == saved, "cancel leaves saved settings intact");
    fs::rename(root / "moved-game", root / "Data");
    Check(Start(root / "data-folder-user"), "automatically find Data folder beside the executable");
    Check(dialogs == 3, "Data folder needs no selector");
    Check(!Platform_WasSetupCanceled(), "new launch resets cancellation state");
    Platform_Shutdown();

    fs::rename(root / "Data", root / "moved-game");
    dialogError = exitOnWarning = true;
    Check(!Start(root / "dialog-error-user"), "folder selector failure returns to setup");
    Check(messages.back().find("No desktop folder selector available") != std::string::npos,
          "capture error from callback thread");
    dialogError = false;
    const fs::path blockedUser = root / "blocked-user";
    Write(blockedUser / "opencmr2.ini", "[video]\nfov = 85\n");
    fs::create_directory(blockedUser / "opencmr2.ini.tmp");
    folders = {(root / "moved-game").u8string()};
    Check(!Start(blockedUser), "settings write failure returns to setup");
    Check(messages.back().find("could not be saved") != std::string::npos &&
          Read(blockedUser / "opencmr2.ini") == "[video]\nfov = 85\n", "failed save preserves original INI");
    exitOnWarning = false;
    const fs::path movedGame = root / "moved-game";
    fs::create_directories(movedGame / "GAME/Tracks/Finland/1");
    for (const char *inside : {"GAME", "GAME/Bigfiles", "GAME/Tracks/Finland/1", "frontend", "Sounds"}) {
        folders = {(movedGame / inside).u8string()};
        const fs::path smartUser = root / "inside-user";
        fs::remove_all(smartUser);
        Check(Start(smartUser), "select a folder inside the installation");
        Check(std::string(Platform_GetDataDir()) == movedGame.u8string() + "/" &&
              Read(smartUser / "opencmr2.ini").find("data = " + movedGame.u8string()) != std::string::npos,
              "save the installation root rather than the chosen subfolder");
        Platform_Shutdown();
    }

    const fs::path collection = root / "collection";
    const fs::path nestedGame = collection / "download" / "CMR2" / "Data";
    fs::create_directories(nestedGame.parent_path());
    fs::rename(movedGame, nestedGame);
    folders = {collection.u8string()};
    Check(Start(root / "parent-user"), "find installation three levels below selected folder");
    Check(std::string(Platform_GetDataDir()) == nestedGame.u8string() + "/", "persist the discovered nested root");
    Platform_Shutdown();

    const fs::path secondGame = collection / "another-copy";
    fs::copy(nestedGame, secondGame, fs::copy_options::recursive);
    folders = {collection.u8string(), (nestedGame / "GAME").u8string()};
    const size_t beforeAmbiguous = messages.size();
    Check(Start(root / "ambiguous-user"), "ask again when several installations are found");
    Check(messages.size() == beforeAmbiguous + 2 &&
          messages.back().find("More than one") != std::string::npos &&
          std::string(Platform_GetDataDir()) == nestedGame.u8string() + "/", "do not choose a copy arbitrarily");
    Platform_Shutdown();

    // A cyclic symlink must not make a broad-folder search loop. Creating
    // symlinks may be disallowed on Windows, so also test the non-link path.
    std::error_code linkError;
    const fs::path noGame = root / "no-game";
    fs::create_directories(noGame / "child");
    fs::create_directory_symlink(noGame, noGame / "child/loop", linkError);
    folders = {noGame.u8string(), nestedGame.u8string()};
    Check(Start(root / "cycle-user"), "reject empty folders and tolerate symlink cycles");
    Platform_Shutdown();
    if (!linkError)
        Check(SDL_RemovePath((noGame / "child/loop").u8string().c_str()), "remove cyclic symlink fixture");

    // Windows registry/standard-folder candidates use the same validated startup
    // path. Inject candidates so tests never select the developer's real game.
    const fs::path autoGame = root / fs::u8path(u8"auto/Colin McRae Rally 2 ü");
    fs::create_directories(autoGame.parent_path());
    fs::copy(nestedGame, autoGame, fs::copy_options::recursive);
    const fs::path autoUser = root / "auto-user";
    Write(autoUser / "opencmr2.ini", "; preserve me\n[video]\nfov = 93\n");
    installCandidates = {(root / "missing").u8string(), (root / "incomplete").u8string(), autoGame.u8string(),
                         (autoGame / ".").u8string()};
    int beforeAuto = dialogs;
    Check(Start(autoUser), "detect one complete installation among stale/invalid/duplicate candidates");
    const std::string autoPath = fs::weakly_canonical(autoGame).generic_u8string() + "/";
    Check(dialogs == beforeAuto && std::string(Platform_GetDataDir()) == autoPath, "auto-detection skips folder dialog and normalizes aliases");
    Check(Read(autoUser / "opencmr2.ini").find("data = " + autoPath) != std::string::npos &&
          Read(autoUser / "opencmr2.ini").find("; preserve me") != std::string::npos &&
          Platform_GetSettingInt("video.fov", 0) == 93, "persist detected UTF-8 path without changing other settings");
    Platform_Shutdown();

    const int scanned = discoveryCalls;
    installCandidates = {secondGame.u8string()};
    Check(Start(autoUser), "remembered installation wins over discovery on later launches");
    Check(discoveryCalls == scanned && dialogs == beforeAuto && std::string(Platform_GetDataDir()) == autoPath,
          "valid saved path prevents a registry search or changed selection");
    Platform_Shutdown();
    Check(Start(root / "explicit-auto-user", nestedGame.u8string()), "explicit path wins over discovered installations");
    Check(discoveryCalls == scanned && dialogs == beforeAuto, "explicit path skips discovery");
    Platform_Shutdown();

    const fs::path movedAuto = root / "auto/moved";
    fs::rename(autoGame, movedAuto);
    installCandidates = {autoGame.u8string(), movedAuto.u8string()};
    Check(Start(autoUser), "rediscover moved installation after saved path disappears");
    Check(dialogs == beforeAuto && std::string(Platform_GetDataDir()) == fs::weakly_canonical(movedAuto).generic_u8string()+"/",
          "update stale saved path automatically");
    Platform_Shutdown();

    installCandidates = {movedAuto.u8string(), secondGame.u8string()};
    folders = {secondGame.u8string()};
    const size_t beforeAutoAmbiguous = messages.size();
    Check(Start(root / "auto-ambiguous-user"), "multiple auto-detected installations need a user choice");
    Check(dialogs == beforeAuto + 1 && messages[beforeAutoAmbiguous].find("More than one") != std::string::npos &&
          std::string(Platform_GetDataDir()) == secondGame.u8string()+"/", "do not silently choose among installations");
    Platform_Shutdown();

    installCandidates = {movedAuto.u8string()};
    const fs::path blockedAuto = root / "auto-blocked-user";
    Write(blockedAuto / "opencmr2.ini", "[video]\nfov = 89\n");
    fs::create_directory(blockedAuto / "opencmr2.ini.tmp");
    exitOnWarning = true;
    Check(!Start(blockedAuto), "failed auto-save is surfaced instead of starting with an unremembered path");
    Check(messages.back().find("could not be saved") != std::string::npos &&
          Read(blockedAuto / "opencmr2.ini") == "[video]\nfov = 89\n", "auto-save failure preserves the existing configuration");
    exitOnWarning = false;
    installCandidates = {(root / "missing").u8string(), (root / "incomplete").u8string()};
    folders = {nestedGame.u8string()};
    Check(Start(root / "auto-missing-user"), "no valid detected game falls back to normal folder selection");
    Platform_Shutdown();
    installCandidates.clear();
#ifdef __linux__
    // Without --user-dir, files that earlier versions kept one level deeper
    // (OpenCMR2/OpenCMR2) move up to the user directory.
    const fs::path xdg = root / "xdg";
    Write(xdg / "OpenCMR2/OpenCMR2/opencmr2.ini", "[paths]\ndata = " + nestedGame.u8string() + "\n");
    Write(xdg / "OpenCMR2/OpenCMR2/Configuration/GameInfo.rcf", "old save");
    // In the process environment: SDL_Quit drops SDL's copy between launches.
    setenv("XDG_DATA_HOME", xdg.u8string().c_str(), 1);
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    char name[] = "game_data_setup";
    char *args[] = {name};
    Check(Platform_Init(1, args), "start from the default user directory");
    Check(std::string(Platform_GetUserDir()) == (xdg / "OpenCMR2").u8string() + "/" &&
          std::string(Platform_GetDataDir()) == nestedGame.u8string() + "/" &&
          Read(xdg / "OpenCMR2/Configuration/GameInfo.rcf") == "old save" &&
          !fs::exists(xdg / "OpenCMR2/OpenCMR2"), "move the old nested user directory");
    Platform_Shutdown();
    // That launch asked once about the applications menu (the mock says Add).
    const fs::path desktop = xdg / "applications/io.github.pablocpas.OpenCMR2.desktop";
    const fs::path icon = xdg / "icons/hicolor/256x256/apps/io.github.pablocpas.OpenCMR2.png";
    const std::string exe = fs::read_symlink("/proc/self/exe").u8string();
    const size_t asked = messages.size();
    Check(fs::exists(icon) && Read(desktop).find("\nExec=\"" + exe + "\"\n") != std::string::npos &&
          Read(desktop).find("\nIcon=" + icon.u8string() + "\n") != std::string::npos &&
          Read(xdg / "OpenCMR2/opencmr2.ini").find("menu_shortcut = 1") != std::string::npos,
          "add the menu entry when accepted");
    Check(Platform_Init(1, args), "start again with the menu entry");
    Check(messages.size() == asked, "ask about the menu entry only once");
    Platform_Shutdown();
    Write(xdg / "OpenCMR2/opencmr2.ini", "[paths]\ndata = " + nestedGame.u8string() + "\n[install]\nmenu_shortcut = 0\n");
    Check(Platform_Init(1, args), "start with the menu entry turned off");
    Check(!fs::exists(desktop) && !fs::exists(icon), "remove the menu entry");
    Platform_Shutdown();
    unsetenv("XDG_DATA_HOME");
#endif
    const int beforeExit = dialogs;
    exitSetup = true;
    Check(!Start(root / "exit-user"), "exit from welcome dialog");
    Check(dialogs == beforeExit, "exit does not open folder selector");
    Check(Platform_WasSetupCanceled(), "welcome exit is a clean exit");
    Platform_SetTestDialogs(nullptr);
    Platform_SetTestInstallCandidates(nullptr);
    fs::current_path(previous);
    fs::remove_all(root);
    puts("game data setup: folder selection, automatic discovery, UTF-8, persistence, precedence, aliases, ambiguity and save failures passed");
}
