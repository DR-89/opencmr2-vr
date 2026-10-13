// Uses the same recorder, GPU executor, OpenXR session and input paths as the game.
// A headset test without requiring or distributing any original game assets.
#include "vr/math.h"
#include "port/gfx.h"
#include "port/sys.h"
#include "port/input.h"
#include "port/vr.h"
#include "platform/platform.h"
#include <vector>

namespace {
void Box(float x,float y,float z,float size,DWORD color) {
    const float points[8][3]={{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
                             {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
    const WORD indices[]={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,
                          3,7,6,3,6,2,0,4,7,0,7,3,1,2,6,1,6,5};
    GfxVertex vertices[8]{};
    for(int i=0;i<8;++i) {
        vertices[i].x=x+points[i][0]*size; vertices[i].y=y+points[i][1]*size;
        vertices[i].z=z+points[i][2]*size; vertices[i].diffuse=color;
    }
    Gfx_DrawIndexedPrimitive(GFX_TRIANGLELIST,vertices,8,indices,36);
}
}
int VR_RunDiagnostic() {
    if (!Sys_CreateWindow("OpenCMR2 VR - tracking and input test",FALSE) || !Gfx_SetVideoMode(1280,960,FALSE)) {
        Gfx_Shutdown(); return 1;
    }
    Sys_Log("vr-test: cubes at 2/4/6 metres. Stick moves white cube; RT raises green, LT raises red. Right stick click / Escape exits.");
    auto identity=VR::Identity(); auto projection=VR::Projection({-.7f,.7f,.6f,-.6f},.05f,100);
    Gfx_SetTransform(GFX_TRANSFORM_WORLD,&identity); Gfx_SetTransform(GFX_TRANSFORM_VIEW,&identity);
    Gfx_SetTransform(GFX_TRANSFORM_PROJECTION,&projection);
    Gfx_SetLighting(FALSE); Gfx_SetCullMode(GFX_CULL_NONE);
    Gfx_SetStageColorOp(0,GFX_TOP_SELECTARG1); Gfx_SetStageColorArg1(0,GFX_TA_DIFFUSE);
    Gfx_SetStageAlphaOp(0,GFX_TOP_SELECTARG1); Gfx_SetStageAlphaArg1(0,GFX_TA_DIFFUSE);
    Gfx_SetDepthTest(TRUE); Gfx_SetDepthWrite(TRUE);
    const char *limitText=SDL_getenv("OPENCMR2_VR_TEST_FRAMES");
    const int frameLimit=limitText ? SDL_atoi(limitText) : 0;
    const char *capturePath=SDL_getenv("OPENCMR2_VR_TEST_CAPTURE");
    std::vector<DWORD> pixels(capturePath ? 1280*960 : 0);
    bool quit=false, captured=false;
    int frames=0;
    while(!quit) {
        SysEvent event;
        while(Sys_PollEvent(&event)) if(event.type==SYS_EVENT_QUIT) quit=true;
        if(Input_IsKeyDown(0x01)) quit=true;
        float steer=0,gas=0,brake=0; VR_GetDriveInput(&steer,&gas,&brake);
        Gfx_Clear(GFX_CLEAR_TARGET|GFX_CLEAR_ZBUFFER,0xff102030,1);
        Gfx_SetVRWorld(TRUE);
        for(int z=2;z<=6;z+=2) {
            Box(-1.4f,-0.6f,(float)z,.25f,0xff3a8bca);
            Box(1.4f,-0.6f,(float)z,.25f,0xffc98d48);
        }
        Box(steer,0,2,.12f,0xffffffff);
        Box(-.4f,-.5f+gas,2,.1f,0xff40e060);
        Box(.4f,-.5f+brake,2,.1f,0xffe05050);
        Gfx_SetVRWorld(FALSE);
        if(capturePath && Gfx_ReadBackBuffer(0,0,1280,960,pixels.data())) captured=true;
        Gfx_Present(FALSE);
        ++frames;
        if(frameLimit>0 && frames>=frameLimit) quit=true;
    }
    int code=0;
    if(capturePath) {
        SDL_Surface *surface=captured ? SDL_CreateSurfaceFrom(1280,960,SDL_PIXELFORMAT_ARGB8888,pixels.data(),1280*4) : nullptr;
        if(!surface || !SDL_SaveBMP(surface,capturePath)) {
            Sys_Log("vr-test: capture failed: %s",SDL_GetError()); code=2;
        }
        if(surface) SDL_DestroySurface(surface);
    }
    Sys_Log("vr-test: completed %d frames",frames);
    Gfx_Shutdown(); return code;
}
