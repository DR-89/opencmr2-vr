#include "vr/runtime.h"
#include "port/vr.h"
#include "platform/platform.h"
#include <cmath>

extern "C" BOOL VR_IsEnabled() {
#ifdef OPENCMR2_VR
    return Platform_GetSettingInt("vr.enabled",0)!=0;
#else
    return FALSE;
#endif
}
namespace {
SDL_Gamepad *pad=nullptr;
Uint64 nextScan=0;
bool usePad=false, previousRecenter=false;
VR::Controls chosen;
bool Active(const VR::Controls &c) {
    return std::fabs(c.steer)>0.1f || std::fabs(c.menuY)>0.5f || c.throttle>0.1f || c.brake>0.1f ||
        c.accept || c.back || c.handbrake || c.gearUp || c.gearDown || c.camera || c.recenter;
}
void Update() {
    chosen={};
    if(!VR_IsEnabled()) return;
    if(pad && !SDL_GamepadConnected(pad)) { SDL_CloseGamepad(pad); pad=nullptr; usePad=false; nextScan=0; }
    if(!pad && SDL_GetTicks()>=nextScan) {
        nextScan=SDL_GetTicks()+1000;
        if (!(SDL_WasInit(SDL_INIT_GAMEPAD) & SDL_INIT_GAMEPAD)) SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        int count=0; auto ids=SDL_GetGamepads(&count);
        const int index=Platform_GetSettingInt("vr.gamepad_index",0);
        if(index>=0 && index<count) pad=SDL_OpenGamepad(ids[index]);
        SDL_free(ids);
    }
    if(!VR::Focused()) { previousRecenter=false; return; }
    VR::Controls usb;
    if(pad) {
        usb.active=true;
        auto axis=[](SDL_GamepadAxis a) { int v=SDL_GetGamepadAxis(pad,a); return v/(v<0?32768.0f:32767.0f); };
        auto button=[](SDL_GamepadButton b) { return SDL_GetGamepadButton(pad,b); };
        usb.steer=VR::Deadzone(axis(SDL_GAMEPAD_AXIS_LEFTX)); usb.menuY=-axis(SDL_GAMEPAD_AXIS_LEFTY);
        usb.throttle=std::max(0.0f,axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
        usb.brake=std::max(0.0f,axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
        usb.accept=button(SDL_GAMEPAD_BUTTON_SOUTH); usb.back=button(SDL_GAMEPAD_BUTTON_RIGHT_STICK);
        usb.handbrake=button(SDL_GAMEPAD_BUTTON_WEST); usb.camera=button(SDL_GAMEPAD_BUTTON_NORTH);
        usb.gearUp=usb.accept; usb.gearDown=button(SDL_GAMEPAD_BUTTON_EAST);
        usb.recenter=button(SDL_GAMEPAD_BUTTON_LEFT_STICK);
        if(button(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) usb.steer=-1;
        if(button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) usb.steer=1;
        if(button(SDL_GAMEPAD_BUTTON_DPAD_UP)) usb.menuY=1;
        if(button(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) usb.menuY=-1;
    }
    const auto &motion=VR::MotionControls();
    // Switch the complete device, never combine one device's brake with another's gas.
    if(Active(usb)) usePad=true;
    else if(Active(motion)) usePad=false;
    chosen=(pad && (usePad || !motion.active))?usb:motion;
    if(chosen.recenter && !previousRecenter) VR::Recenter();
    previousRecenter=chosen.recenter;
}
}
extern "C" BOOL VR_GetDriveInput(float *steer,float *throttle,float *brake) {
    Update();
    // Neutralize the original joystick path as well when the headset loses focus.
    if (VR_IsEnabled() && !VR::Focused()) { *steer=*throttle=*brake=0; return TRUE; }
    if(!chosen.active) return FALSE;
    *steer=chosen.steer; *throttle=chosen.throttle; *brake=chosen.brake;
    return TRUE;
}
extern "C" void VR_MergeKeyboard(BYTE keys[256]) {
    Update();
    if(!chosen.active) return;
    auto key=[&](int k,bool down) { if(down) keys[k]=0x80; };
    key(0xcb,chosen.steer<-0.5f); key(0xcd,chosen.steer>0.5f);
    key(0xc8,chosen.menuY>0.5f); key(0xd0,chosen.menuY<-0.5f);
    key(0x1c,chosen.accept); key(0x01,chosen.back); key(0x39,chosen.handbrake);
    key(0x1b,chosen.gearUp); key(0x1a,chosen.gearDown); key(0x2e,chosen.camera);
}
extern "C" void VR_ShutdownInput() {
    if(pad) SDL_CloseGamepad(pad);
    pad=nullptr; nextScan=0; chosen={}; usePad=previousRecenter=false;
}
