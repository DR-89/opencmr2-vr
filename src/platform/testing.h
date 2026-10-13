// Overrides used by the stage probe and startup tests. Normal play uses SDL.
#ifndef OPENCMR2_PLATFORM_TESTING_H
#define OPENCMR2_PLATFORM_TESTING_H
#include "port/types.h"
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_messagebox.h>
#include <string>
#include <vector>
struct PlatformTestClock {
    DWORD (*ticks)();
    void (*sleep)(DWORD);
    unsigned int unixTime;
};
void Platform_SetTestClock(const PlatformTestClock *clock);
void Platform_SetTestKeyboard(const BYTE *keys); // 256 DIK entries, NULL disables
// Startup tests provide folder results without opening desktop dialogs.
struct PlatformTestDialogs {
    void (*openFolder)(SDL_DialogFileCallback, void *);
    bool (*messageBox)(const SDL_MessageBoxData *, int *);
};
void Platform_SetTestDialogs(const PlatformTestDialogs *dialogs);
// An empty provider isolates startup tests from real installations on the host.
void Platform_SetTestInstallCandidates(std::vector<std::string> (*provider)());
#endif
