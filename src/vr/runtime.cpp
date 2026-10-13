// OpenXR lifecycle and compositor submission shared by Android and desktop.
// SDL owns the Vulkan device and wraps runtime swapchain images as GPU textures.
#include <openxr/openxr.h>
#include <SDL3/SDL_openxr.h>
#include "vr/runtime.h"
#include "port/vr.h"
#include "port/sys.h"
#include "platform/platform.h"
#include <cstring>
#include <vector>

namespace VR {
namespace {
#define XR_FUNCTIONS(X) \
 X(DestroyInstance) X(GetInstanceProperties) X(EnumerateViewConfigurationViews) \
 X(EnumerateEnvironmentBlendModes) X(CreateReferenceSpace) X(DestroySpace) \
 X(DestroySession) X(PollEvent) X(BeginSession) X(EndSession) X(WaitFrame) X(BeginFrame) X(EndFrame) \
 X(LocateViews) X(LocateSpace) X(AcquireSwapchainImage) X(WaitSwapchainImage) X(ReleaseSwapchainImage) \
 X(StringToPath) X(CreateActionSet) X(DestroyActionSet) X(CreateAction) \
 X(SuggestInteractionProfileBindings) X(AttachSessionActionSets) X(SyncActions) \
 X(GetActionStateFloat) X(GetActionStateBoolean) X(GetActionStateVector2f)
#define DECLARE(name) PFN_xr##name xr##name = nullptr;
XR_FUNCTIONS(DECLARE)
#undef DECLARE
XrInstance instance=XR_NULL_HANDLE;
XrSystemId systemId=XR_NULL_SYSTEM_ID;
XrSession session=XR_NULL_HANDLE;
XrSpace localSpace=XR_NULL_HANDLE, headSpace=XR_NULL_HANDLE;
XrActionSet actionSet=XR_NULL_HANDLE;
SDL_GPUDevice *gpu=nullptr;
XrSessionState state=XR_SESSION_STATE_UNKNOWN;
bool running=false, begun=false, render=false, failed=false, needOrigin=true;
XrFrameState frameState{XR_TYPE_FRAME_STATE};
XrView views[2]={{XR_TYPE_VIEW},{XR_TYPE_VIEW}};
Eye eyes[2];
Pose origin;
Controls motion;
SDL_GPUTextureFormat format=SDL_GPU_TEXTUREFORMAT_INVALID;
struct Swapchain {
    XrSwapchain handle=XR_NULL_HANDLE;
    SDL_GPUTexture **images=nullptr;
    uint32_t index=0;
    bool acquired=false, waited=false;
    int width=0,height=0;
} swaps[3];
XrAction steer,throttle,brake,accept,back,handbrake,gearDown,camera,recenter;
std::vector<const char *> optionalExtensions;
PFN_xrEnumerateDisplayRefreshRatesFB enumerateRefreshRates=nullptr;
PFN_xrRequestDisplayRefreshRateFB requestRefreshRate=nullptr;
PFN_xrGetDisplayRefreshRateFB getRefreshRate=nullptr;
PFN_xrPerfSettingsSetPerformanceLevelEXT setPerformanceLevel=nullptr;

void ConfigurePerformance() {
    // PORT: request sustained operation, leaving thermal limits to the runtime.
    if (setPerformanceLevel) {
        for (auto domain : {XR_PERF_SETTINGS_DOMAIN_CPU_EXT,XR_PERF_SETTINGS_DOMAIN_GPU_EXT}) {
            auto r=setPerformanceLevel(session,domain,XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT);
            Sys_Log("vr: sustained performance domain %d result %d",(int)domain,(int)r);
        }
    }
#ifdef __ANDROID__
    constexpr int defaultRefresh=120;
#else
    constexpr int defaultRefresh=0; // PCVR keeps the user's runtime setting.
#endif
    const int wanted=Platform_GetSettingInt("vr.refresh_hz",defaultRefresh);
    if (enumerateRefreshRates && requestRefreshRate && wanted>0) {
        uint32_t count=0;
        if (XR_SUCCEEDED(enumerateRefreshRates(session,0,&count,nullptr))) {
            std::vector<float> rates(count);
            if (XR_SUCCEEDED(enumerateRefreshRates(session,count,&count,rates.data()))) {
                float selected=0;
                for(float rate:rates) {
                    Sys_Log("vr: supported refresh %.1f Hz",rate);
                    if(rate<=wanted && rate>selected) selected=rate;
                }
                if(selected>0) Sys_Log("vr: request %.1f Hz result %d",selected,(int)requestRefreshRate(session,selected));
            }
        }
    }
    if(getRefreshRate) {
        float current=0;
        if(XR_SUCCEEDED(getRefreshRate(session,&current))) Sys_Log("vr: current refresh %.1f Hz",current);
    }
}

bool Check(XrResult r,const char *operation) {
    if (XR_SUCCEEDED(r)) return true;
    Sys_Log("vr: %s failed (%d)",operation,(int)r);
    return false;
}
void Quit() {
    failed=true; render=false; motion={};
    SDL_Event event{}; event.type=SDL_EVENT_QUIT; SDL_PushEvent(&event);
}
Pose ToPose(const XrPosef &p) { return {{p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w},{p.position.x,p.position.y,p.position.z}}; }
XrPosef ToXrPose(const Pose &p) { return {{p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w},{p.position.x,p.position.y,p.position.z}}; }
XrPath Path(const char *s) {
    XrPath p=XR_NULL_PATH;
    Check(xrStringToPath(instance,s,&p),s);
    return p;
}
bool MakeAction(XrAction &a,const char *name,XrActionType type) {
    XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};
    SDL_strlcpy(info.actionName,name,sizeof(info.actionName));
    SDL_strlcpy(info.localizedActionName,name,sizeof(info.localizedActionName));
    info.actionType=type;
    return Check(xrCreateAction(actionSet,&info,&a),name);
}
bool SetupActions() {
    XrActionSetCreateInfo info{XR_TYPE_ACTION_SET_CREATE_INFO};
    SDL_strlcpy(info.actionSetName,"driving",sizeof(info.actionSetName));
    SDL_strlcpy(info.localizedActionSetName,"Driving and menus",sizeof(info.localizedActionSetName));
    if (!Check(xrCreateActionSet(instance,&info,&actionSet),"create action set")) return false;
    if (!MakeAction(steer,"steer",XR_ACTION_TYPE_VECTOR2F_INPUT) ||
        !MakeAction(throttle,"throttle",XR_ACTION_TYPE_FLOAT_INPUT) ||
        !MakeAction(brake,"brake",XR_ACTION_TYPE_FLOAT_INPUT)) return false;
    for (auto pair : {std::pair<XrAction*,const char*>{&accept,"accept"},{&back,"back_pause"},
                     {&handbrake,"handbrake"},{&gearDown,"gear_down"},{&camera,"camera"},{&recenter,"recenter"}})
        if (!MakeAction(*pair.first,pair.second,XR_ACTION_TYPE_BOOLEAN_INPUT)) return false;
    std::vector<XrActionSuggestedBinding> bindings={
        {steer,Path("/user/hand/left/input/thumbstick")},
        {throttle,Path("/user/hand/right/input/trigger/value")},
        {brake,Path("/user/hand/left/input/trigger/value")},
        {accept,Path("/user/hand/right/input/a/click")},
        {gearDown,Path("/user/hand/right/input/b/click")},
        {back,Path("/user/hand/right/input/thumbstick/click")},
        {handbrake,Path("/user/hand/right/input/squeeze/value")},
        {camera,Path("/user/hand/left/input/y/click")},
        {recenter,Path("/user/hand/left/input/thumbstick/click")}
    };
    XrInteractionProfileSuggestedBinding suggest{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggest.interactionProfile=Path("/interaction_profiles/oculus/touch_controller");
    suggest.countSuggestedBindings=(uint32_t)bindings.size(); suggest.suggestedBindings=bindings.data();
    if (!Check(xrSuggestInteractionProfileBindings(instance,&suggest),"Touch controller bindings")) return false;
    XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attach.countActionSets=1; attach.actionSets=&actionSet;
    return Check(xrAttachSessionActionSets(session,&attach),"attach actions");
}
bool Boolean(XrAction action) {
    XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO}; get.action=action;
    XrActionStateBoolean value{XR_TYPE_ACTION_STATE_BOOLEAN};
    return XR_SUCCEEDED(xrGetActionStateBoolean(session,&get,&value)) && value.isActive && value.currentState;
}
float Float(XrAction action) {
    XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO}; get.action=action;
    XrActionStateFloat value{XR_TYPE_ACTION_STATE_FLOAT};
    return XR_SUCCEEDED(xrGetActionStateFloat(session,&get,&value)) && value.isActive ? std::clamp(value.currentState,0.0f,1.0f) : 0;
}
void SyncInput() {
    motion={};
    if (state!=XR_SESSION_STATE_FOCUSED) return;
    XrActiveActionSet active{actionSet,XR_NULL_PATH};
    XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO}; sync.countActiveActionSets=1; sync.activeActionSets=&active;
    if (!Check(xrSyncActions(session,&sync),"sync actions")) return;
    XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO}; get.action=steer;
    XrActionStateVector2f value{XR_TYPE_ACTION_STATE_VECTOR2F};
    if (XR_SUCCEEDED(xrGetActionStateVector2f(session,&get,&value)) && value.isActive) {
        motion.active=true; motion.steer=Deadzone(value.currentState.x); motion.menuY=value.currentState.y;
    }
    motion.throttle=Float(throttle); motion.brake=Float(brake);
    motion.accept=Boolean(accept); motion.back=Boolean(back);
    motion.gearUp=motion.accept; motion.gearDown=Boolean(gearDown);
    motion.handbrake=Boolean(handbrake); motion.camera=Boolean(camera);
    motion.recenter=Boolean(recenter);
}
bool MakeSwap(Swapchain &s,int w,int h) {
    XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    info.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
    info.sampleCount=1; info.width=w; info.height=h; info.faceCount=1; info.arraySize=1; info.mipCount=1;
    s.width=w; s.height=h;
    return Check(SDL_CreateGPUXRSwapchain(gpu,session,&info,format,&s.handle,&s.images),"create swapchain");
}
void Release(Swapchain &s) {
    if (s.waited) {
        XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        if (!Check(xrReleaseSwapchainImage(s.handle,&release),"release swapchain")) Quit();
        s.acquired=s.waited=false;
    }
}
bool Acquire(Swapchain &s) {
    XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    if (!Check(xrAcquireSwapchainImage(s.handle,&acquire,&s.index),"acquire swapchain")) return false;
    s.acquired=true;
    XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO}; wait.timeout=XR_INFINITE_DURATION;
    if (!Check(xrWaitSwapchainImage(s.handle,&wait),"wait swapchain")) return false;
    s.waited=true; return true;
}
}

void ConfigureDevice(SDL_PropertiesID props) {
    if (!VR_IsEnabled()) return;
    SDL_SetStringProperty(props,SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,"vulkan");
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_XR_ENABLE_BOOLEAN,true);
    SDL_SetPointerProperty(props,SDL_PROP_GPU_DEVICE_CREATE_XR_INSTANCE_POINTER,&instance);
    SDL_SetPointerProperty(props,SDL_PROP_GPU_DEVICE_CREATE_XR_SYSTEM_ID_POINTER,&systemId);
    SDL_SetNumberProperty(props,SDL_PROP_GPU_DEVICE_CREATE_XR_VERSION_NUMBER,XR_MAKE_VERSION(1,0,0));
    SDL_SetStringProperty(props,SDL_PROP_GPU_DEVICE_CREATE_XR_APPLICATION_NAME_STRING,"OpenCMR2 VR");
    // Enable optional extensions only when advertised by this OpenXR runtime.
    optionalExtensions.clear();
    if(SDL_OpenXR_LoadLibrary()) {
        auto get=SDL_OpenXR_GetXrGetInstanceProcAddr();
        PFN_xrEnumerateInstanceExtensionProperties enumerate=nullptr;
        uint32_t count=0;
        if(get && XR_SUCCEEDED(get(XR_NULL_HANDLE,"xrEnumerateInstanceExtensionProperties",(PFN_xrVoidFunction*)&enumerate)) &&
           enumerate && XR_SUCCEEDED(enumerate(nullptr,0,&count,nullptr))) {
            std::vector<XrExtensionProperties> extensions(count,{XR_TYPE_EXTENSION_PROPERTIES});
            if(XR_SUCCEEDED(enumerate(nullptr,count,&count,extensions.data()))) {
                for(const char *name : {XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME,XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME})
                    for(const auto &extension:extensions) if(strcmp(name,extension.extensionName)==0) {
                        optionalExtensions.push_back(name); break;
                    }
            }
        }
        SDL_OpenXR_UnloadLibrary();
    }
    SDL_SetNumberProperty(props,SDL_PROP_GPU_DEVICE_CREATE_XR_EXTENSION_COUNT_NUMBER,optionalExtensions.size());
    SDL_SetPointerProperty(props,SDL_PROP_GPU_DEVICE_CREATE_XR_EXTENSION_NAMES_POINTER,optionalExtensions.data());
}

bool Initialize(SDL_GPUDevice *device) {
    if (!VR_IsEnabled()) return true;
    gpu=device;
    if (!instance || !systemId) return false;
    auto get=SDL_OpenXR_GetXrGetInstanceProcAddr();
    if (!get) return false;
#define LOAD(name) if (!Check(get(instance,"xr" #name,(PFN_xrVoidFunction*)&xr##name),"load xr" #name)) return false;
    XR_FUNCTIONS(LOAD)
#undef LOAD
    for(const char *name:optionalExtensions) {
        if(strcmp(name,XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME)==0) {
            get(instance,"xrEnumerateDisplayRefreshRatesFB",(PFN_xrVoidFunction*)&enumerateRefreshRates);
            get(instance,"xrRequestDisplayRefreshRateFB",(PFN_xrVoidFunction*)&requestRefreshRate);
            get(instance,"xrGetDisplayRefreshRateFB",(PFN_xrVoidFunction*)&getRefreshRate);
        } else if(strcmp(name,XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME)==0)
            get(instance,"xrPerfSettingsSetPerformanceLevelEXT",(PFN_xrVoidFunction*)&setPerformanceLevel);
    }
    XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
    if (Check(xrGetInstanceProperties(instance,&properties),"runtime properties"))
        Sys_Log("vr: OpenXR runtime %s",properties.runtimeName);
    uint32_t count=0;
    if (!Check(xrEnumerateEnvironmentBlendModes(instance,systemId,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&count,nullptr),"blend modes")) return false;
    std::vector<XrEnvironmentBlendMode> modes(count);
    if (!Check(xrEnumerateEnvironmentBlendModes(instance,systemId,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,count,&count,modes.data()),"blend modes")) return false;
    if (std::find(modes.begin(),modes.end(),XR_ENVIRONMENT_BLEND_MODE_OPAQUE)==modes.end()) {
        Sys_Log("vr: this build requires an opaque VR headset"); return false;
    }
    XrSessionCreateInfo create{XR_TYPE_SESSION_CREATE_INFO};
    if (!Check(SDL_CreateGPUXRSession(device,&create,&session),"create session")) return false;
    XrReferenceSpaceCreateInfo space{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    space.poseInReferenceSpace.orientation.w=1; space.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;
    if (!Check(xrCreateReferenceSpace(session,&space,&localSpace),"local space")) return false;
    space.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;
    if (!Check(xrCreateReferenceSpace(session,&space,&headSpace),"head space")) return false;
    if (!SetupActions()) return false;
    count=0;
    if (!Check(xrEnumerateViewConfigurationViews(instance,systemId,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&count,nullptr),"view count") || count!=2) return false;
    XrViewConfigurationView config[2]={{XR_TYPE_VIEW_CONFIGURATION_VIEW},{XR_TYPE_VIEW_CONFIGURATION_VIEW}};
    if (!Check(xrEnumerateViewConfigurationViews(instance,systemId,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&count,config),"views")) return false;
    int numFormats=0;
    auto formats=SDL_GetGPUXRSwapchainFormats(device,session,&numFormats);
    for (auto wanted : {SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                        SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB}) {
        for(int i=0;i<numFormats;++i) if (formats[i]==wanted) format=wanted;
        if(format!=SDL_GPU_TEXTUREFORMAT_INVALID) break;
    }
    SDL_free(formats);
    if(format==SDL_GPU_TEXTUREFORMAT_INVALID) { Sys_Log("vr: no compatible swapchain format"); return false; }
    const float scale=std::clamp(Platform_GetSettingInt("vr.resolution_percent",80),40,150)/100.0f;
    for(int i=0;i<2;++i) {
        eyes[i].width=std::min((uint32_t)(config[i].recommendedImageRectWidth*scale),config[i].maxImageRectWidth);
        eyes[i].height=std::min((uint32_t)(config[i].recommendedImageRectHeight*scale),config[i].maxImageRectHeight);
        if(!MakeSwap(swaps[i],eyes[i].width,eyes[i].height)) return false;
    }
    if(!MakeSwap(swaps[2],1280,960)) return false;
    Sys_Log("vr: stereo %dx%d / %dx%d",eyes[0].width,eyes[0].height,eyes[1].width,eyes[1].height);
    return true;
}

void BeginFrame() {
    if(!session || begun || failed) return;
    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    XrResult result;
    while((result=xrPollEvent(instance,&event))==XR_SUCCESS) {
        if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            const auto &changed=*(const XrEventDataSessionStateChanged*)&event;
            state=changed.state;
            Sys_Log("vr: session state %d",(int)state);
            if(state==XR_SESSION_STATE_READY && !running) {
                XrSessionBeginInfo start{XR_TYPE_SESSION_BEGIN_INFO}; start.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                running=Check(xrBeginSession(session,&start),"begin session"); needOrigin=true;
                if(!running) Quit();
                else ConfigurePerformance();
            } else if(state==XR_SESSION_STATE_STOPPING && running) {
                Check(xrEndSession(session),"end session"); running=false; motion={};
            } else if(state==XR_SESSION_STATE_EXITING || state==XR_SESSION_STATE_LOSS_PENDING) Quit();
        } else if(event.type==XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB) {
            const auto &change=*(const XrEventDataDisplayRefreshRateChangedFB*)&event;
            Sys_Log("vr: refresh changed %.1f -> %.1f Hz",change.fromDisplayRefreshRate,change.toDisplayRefreshRate);
        } else if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) Quit();
        else if(event.type==XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) needOrigin=true;
        event={XR_TYPE_EVENT_DATA_BUFFER};
    }
    if(result!=XR_EVENT_UNAVAILABLE && !Check(result,"poll events")) Quit();
    if(!running || failed) { SDL_Delay(10); return; }
    XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};
    frameState={XR_TYPE_FRAME_STATE};
    if(!Check(xrWaitFrame(session,&wait,&frameState),"wait frame")) { Quit(); return; }
    XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};
    if(!Check(xrBeginFrame(session,&begin),"begin frame")) { Quit(); return; }
    begun=true; render=false;
    SyncInput();
    if(!frameState.shouldRender) return;
    XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};
    locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    locate.displayTime=frameState.predictedDisplayTime; locate.space=localSpace;
    XrViewState viewState{XR_TYPE_VIEW_STATE}; uint32_t count=0;
    if(!Check(xrLocateViews(session,&locate,&viewState,2,&count,views),"locate views") || count!=2) return;
    const XrViewStateFlags valid=XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_VALID_BIT;
    if((viewState.viewStateFlags&valid)!=valid) return;
    if(needOrigin) {
        XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};
        if(!Check(xrLocateSpace(headSpace,localSpace,frameState.predictedDisplayTime,&head),"locate head")) return;
        const XrSpaceLocationFlags flags=XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_VALID_BIT;
        if((head.locationFlags&flags)!=flags) return;
        origin=SeatedOrigin(ToPose(head.pose)); needOrigin=false;
    }
    float units=std::clamp(Platform_GetSettingInt("vr.world_scale_percent",100),10,1000)/100.0f;
    for(int i=0;i<2;++i) {
        const auto &f=views[i].fov;
        eyes[i].view=EyeFromGame(origin,ToPose(views[i].pose),units);
        eyes[i].projection=Projection({f.angleLeft,f.angleRight,f.angleUp,f.angleDown},0.05f*units,10000.0f*units);
    }
    render=true;
}

bool Rendering() { return render && begun; }
SDL_GPUTextureFormat ColorFormat() { return format; }
bool AcquireEyes() {
    if(!Rendering()) return false;
    for(int i=0;i<2;++i) {
        if(!swaps[i].waited && !Acquire(swaps[i])) { Quit(); return false; }
        eyes[i].texture=swaps[i].images[swaps[i].index];
    }
    return true;
}
const Eye &GetEye(int i) { return eyes[i]; }
const Controls &MotionControls() { return motion; }
bool Focused() { return running && state==XR_SESSION_STATE_FOCUSED && !failed; }
void Recenter() { needOrigin=true; }

void EndFrame(bool world,SDL_GPUTexture *menu,int w,int h) {
    if(!begun) return;
    XrCompositionLayerProjectionView projectionViews[2]={{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}};
    XrCompositionLayerProjection projection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
    const XrCompositionLayerBaseHeader *layer=nullptr;
    if(render) {
        // PORT: world rendering already targets acquired OpenXR images. Avoid
        // two full-size eye copies and their extra queue submission each frame.
        bool ok=world ? swaps[0].waited && swaps[1].waited : Acquire(swaps[2]);
        SDL_GPUCommandBuffer *cmd=ok && !world ? SDL_AcquireGPUCommandBuffer(gpu) : nullptr;
        if(!world && cmd) {
                SDL_GPUBlitInfo blit{};
                blit.source.texture=menu;
                blit.source.w=w; blit.source.h=h;
                blit.destination.texture=swaps[2].images[swaps[2].index];
                blit.destination.w=swaps[2].width; blit.destination.h=swaps[2].height;
                blit.load_op=SDL_GPU_LOADOP_DONT_CARE; blit.filter=SDL_GPU_FILTER_LINEAR;
                SDL_BlitGPUTexture(cmd,&blit);
            // SDL submits to the same Vulkan queue given to OpenXR and restores
            // these color-target images to COLOR_ATTACHMENT_OPTIMAL. Release
            // after submission: the runtime synchronizes GPU completion. Queue
            // access stays on this thread; a CPU fence here serializes frames.
            // https://github.com/KhronosGroup/OpenXR-Docs/issues/177
            ok=SDL_SubmitGPUCommandBuffer(cmd);
        } else if(!world) ok=false;
        for(auto &s:swaps) Release(s);
        if(ok && !failed) {
            if(world) {
                for(int i=0;i<2;++i) {
                    auto &v=projectionViews[i]; v.pose=views[i].pose; v.fov=views[i].fov;
                    v.subImage.swapchain=swaps[i].handle;
                    v.subImage.imageRect.extent={eyes[i].width,eyes[i].height};
                }
                projection.space=localSpace; projection.viewCount=2; projection.views=projectionViews;
                layer=(const XrCompositionLayerBaseHeader*)&projection;
            } else {
                quad.space=localSpace; quad.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
                quad.subImage.swapchain=swaps[2].handle; quad.subImage.imageRect.extent={1280,960};
                Pose panel=origin; const auto m=PoseMatrix(origin);
                panel.position.x-=m._31*2.5f; panel.position.y-=m._32*2.5f; panel.position.z-=m._33*2.5f;
                quad.pose=ToXrPose(panel); quad.size={3.0f,3.0f*h/w};
                layer=(const XrCompositionLayerBaseHeader*)&quad;
            }
        } else if(!ok) Quit();
    }
    XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
    end.displayTime=frameState.predictedDisplayTime; end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    end.layerCount=layer?1:0; end.layers=layer?&layer:nullptr;
    if(!Check(xrEndFrame(session,&end),"end frame")) Quit();
    begun=false; render=false;
    eyes[0].texture=eyes[1].texture=nullptr;
}

void Shutdown(SDL_GPUDevice *device) {
    if(!gpu) return;
    SDL_WaitForGPUIdle(device);
    if(begun) { render=false; EndFrame(false,nullptr,1,1); }
    for(auto &s:swaps) {
        if(s.handle) SDL_DestroyGPUXRSwapchain(device,s.handle,s.images);
        s={};
    }
    if(headSpace && xrDestroySpace) xrDestroySpace(headSpace);
    if(localSpace && xrDestroySpace) xrDestroySpace(localSpace);
    if(session && xrDestroySession) xrDestroySession(session);
    if(actionSet && xrDestroyActionSet) xrDestroyActionSet(actionSet);
    if(instance && xrDestroyInstance) xrDestroyInstance(instance);
    session=XR_NULL_HANDLE; instance=XR_NULL_HANDLE; localSpace=headSpace=XR_NULL_HANDLE; actionSet=XR_NULL_HANDLE;
    gpu=nullptr; motion={}; running=begun=render=failed=false; needOrigin=true;
    state=XR_SESSION_STATE_UNKNOWN; format=SDL_GPU_TEXTUREFORMAT_INVALID;
    enumerateRefreshRates=nullptr; requestRefreshRate=nullptr; getRefreshRate=nullptr;
    setPerformanceLevel=nullptr; optionalExtensions.clear();
    VR_ShutdownInput();
}
}
