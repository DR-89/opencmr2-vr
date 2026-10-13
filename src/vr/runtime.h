#pragma once
#include "vr/math.h"
#include <SDL3/SDL.h>

namespace VR {
struct Controls {
    bool active=false;
    float steer=0, menuY=0, throttle=0, brake=0;
    bool accept=false, back=false, handbrake=false, gearUp=false, gearDown=false, camera=false, recenter=false;
};
struct Eye {
    int width=0,height=0;
    GfxMatrix view=Identity(), projection=Identity();
    SDL_GPUTexture *texture=nullptr;
};
#ifdef OPENCMR2_VR
void ConfigureDevice(SDL_PropertiesID props);
bool Initialize(SDL_GPUDevice *device);
void Shutdown(SDL_GPUDevice *device);
void BeginFrame(); // idempotent until EndFrame: also handles intermediate game flushes
bool Rendering();
const Eye &GetEye(int index);
void EndFrame(bool world, SDL_GPUTexture *menu, SDL_GPUTexture *left, SDL_GPUTexture *right, int w, int h);
const Controls &MotionControls();
bool Focused();
void Recenter();
#else
inline void ConfigureDevice(SDL_PropertiesID) {}
inline bool Initialize(SDL_GPUDevice *) { return true; }
inline void Shutdown(SDL_GPUDevice *) {}
inline void BeginFrame() {}
inline bool Rendering() { return false; }
inline const Eye &GetEye(int) { static Eye e; return e; }
inline void EndFrame(bool,SDL_GPUTexture *,SDL_GPUTexture *,SDL_GPUTexture *,int,int) {}
inline const Controls &MotionControls() { static Controls c; return c; }
inline bool Focused() { return false; }
inline void Recenter() {}
#endif
}
