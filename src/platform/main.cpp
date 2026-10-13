// Process entry point: sets up the platform and runs the game's main loop
// (CMain::Initialize, which was WinMain's body).

#include "platform/platform.h"
#include "port/sys.h"
#include "diagnostics/timing.h"
#include "main.h"
int VR_RunDiagnostic();

// On Android, SDLActivity calls SDL_main, which SDL_main.h renames main to.
#ifndef __ANDROID__
#define SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL_main.h>

int main(int argc, char **argv)
{
    SDL_SetMainReady();
    if (!Platform_Init(argc, argv))
        return Platform_WasSetupCanceled() ? 0 : 1;
    if (Platform_GetSettingInt("vr.diagnostic",0)) {
        int code=VR_RunDiagnostic();
        Platform_Shutdown();
        return code;
    }
    Diagnostics::StartProfileFromEnvironment();
    unsigned int code = CMain::Initialize(Sys_GetCommandLine());
    Diagnostics::StopProfile();
    Platform_Shutdown();
    return (int)code;
}
