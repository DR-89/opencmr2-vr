#include "vr/runtime.h"
#include "port/vr.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static bool enabled=true, focused=true;
static int recenterCount=0;
static int gamepadIndex=0;
static VR::Controls motion;
int Platform_GetSettingInt(const char *key,int) {
    return std::strcmp(key,"vr.gamepad_index")==0 ? gamepadIndex : (enabled?1:0);
}
namespace VR {
const Controls &MotionControls() { return motion; }
bool Focused() { return focused; }
void Recenter() { ++recenterCount; }
}
static void Require(bool condition,const char *message) {
    if(!condition) { std::fprintf(stderr,"%s (SDL: %s)\n",message,SDL_GetError()); std::exit(1); }
}
int main() {
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    Require(SDL_Init(SDL_INIT_GAMEPAD),"init SDL gamepad");
    motion.active=true; motion.steer=-.7f; motion.throttle=.4f; motion.accept=motion.gearUp=true;
    float s=0,t=0,b=0;
    Require(VR_GetDriveInput(&s,&t,&b) && s==-.7f && t==.4f && b==0,"Touch analog values");
    BYTE keys[256]{}; VR_MergeKeyboard(keys);
    Require(keys[0xcb] && keys[0x1c],"Touch menu keys");
    Require(keys[0x1b] && !keys[0x1a] && !keys[0x01],"Touch A confirms and shifts up without pausing");
    motion.accept=motion.gearUp=false; motion.gearDown=true;
    std::memset(keys,0,sizeof(keys)); VR_MergeKeyboard(keys);
    Require(keys[0x1a] && !keys[0x1b] && !keys[0x01] && !keys[0x1c],"Touch B shifts down without pausing or confirming");
    motion.gearDown=false; motion.back=true;
    std::memset(keys,0,sizeof(keys)); VR_MergeKeyboard(keys);
    Require(keys[0x01] && !keys[0x1a] && !keys[0x1b],"Touch right stick click opens menu without shifting");
    motion={}; motion.active=true; motion.recenter=true;
    VR_MergeKeyboard(keys); VR_MergeKeyboard(keys);
    Require(recenterCount==1,"recenter is edge triggered");
    focused=false; s=t=b=1;
    Require(VR_GetDriveInput(&s,&t,&b) && s==0 && t==0 && b==0,"focus loss neutralizes driving");
    BYTE neutral[256]{}; VR_MergeKeyboard(neutral);
    Require(neutral[0x1c]==0,"focus loss releases menu keys");
    focused=true; motion={}; motion.active=true;
    SDL_VirtualJoystickDesc desc{}; SDL_INIT_INTERFACE(&desc);
    desc.type=SDL_JOYSTICK_TYPE_GAMEPAD; desc.name="OpenCMR2 VR test gamepad";
    desc.naxes=SDL_GAMEPAD_AXIS_COUNT; desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
    desc.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;
    desc.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
    auto id=SDL_AttachVirtualJoystick(&desc); Require(id!=0,"attach USB substitute");
    int count=0; auto ids=SDL_GetGamepads(&count);
    for(int i=0;i<count;++i) if(ids[i]==id) gamepadIndex=i;
    SDL_free(ids);
    VR_ShutdownInput(); // select our test device even when real gamepads are attached
    auto joy=SDL_OpenJoystick(id); Require(joy!=nullptr,"open USB substitute");
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFT_TRIGGER,-32768);
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,-32768);
    SDL_UpdateJoysticks();
    VR_GetDriveInput(&s,&t,&b); // open at neutral; SDL calibrates trigger resting values
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,32767);
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFTX,32767);
    SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_SOUTH,true);
    SDL_UpdateJoysticks(); SDL_Delay(1050);
    bool hasDrive=VR_GetDriveInput(&s,&t,&b);
    std::printf("USB test values: active=%d steer=%f throttle=%f brake=%f\n",hasDrive,s,t,b);
    Require(hasDrive && s>.99f && t>.99f && b==0,"USB hotplug, steer and trigger");
    SDL_UpdateJoysticks(); std::memset(keys,0,sizeof(keys)); VR_MergeKeyboard(keys);
    Require(keys[0x1b] && keys[0x1c] && !keys[0x1a] && !keys[0x01],"USB A confirms and shifts up without pausing");
    SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_SOUTH,false);
    SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_EAST,true);
    SDL_UpdateJoysticks(); std::memset(keys,0,sizeof(keys)); VR_MergeKeyboard(keys);
    Require(keys[0x1a] && !keys[0x1b] && !keys[0x01] && !keys[0x1c],"USB B shifts down without pausing or confirming");
    SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_EAST,false);
    SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_RIGHT_STICK,true);
    SDL_UpdateJoysticks(); std::memset(keys,0,sizeof(keys)); VR_MergeKeyboard(keys);
    Require(keys[0x01] && !keys[0x1a] && !keys[0x1b],"USB right stick click opens menu without shifting");
    SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_RIGHT_STICK,false);
    for(Sint16 y : {Sint16(-32768),Sint16(0),Sint16(32767)}) {
        SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_RIGHTY,y);
        SDL_UpdateJoysticks(); std::memset(keys,0,sizeof(keys)); VR_MergeKeyboard(keys);
        Require(!keys[0x01] && !keys[0x1a] && !keys[0x1b],"USB right stick movement does not shift or pause");
    }
    motion.brake=1;
    Require(VR_GetDriveInput(&s,&t,&b) && t>.99f && b==0,"do not mix two devices' axes");
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFTX,0);
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,-32768);
    SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_SOUTH,false);
    SDL_UpdateJoysticks();
    Require(VR_GetDriveInput(&s,&t,&b) && b==1 && t==0,"switch back to Touch");
    SDL_DetachVirtualJoystick(id); SDL_CloseJoystick(joy); SDL_UpdateJoysticks();
    motion={}; motion.active=true;
    Require(VR_GetDriveInput(&s,&t,&b) && s==0 && t==0 && b==0,"unplug clears held USB input");
    enabled=false;
    Require(!VR_GetDriveInput(&s,&t,&b),"flat mode does not override driving");
    VR_ShutdownInput(); SDL_Quit();
    std::puts("VR Touch, USB hotplug, arbitration, focus loss and recenter tests passed.");
}
