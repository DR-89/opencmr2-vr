#include "render/frustum.h"
#include "vr/math.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
static void Check(bool condition,const char *message) {
    if(!condition) { std::fprintf(stderr,"tracked culling: %s\n",message); std::exit(1); }
}
static GfxBounds Box(float x,float y,float z,float radius) {
    return {{x,y,z},{radius,radius,radius},true};
}
int main() {
    const auto projection=VR::Projection({-.8f,.7f,.75f,-.7f},.05f,1000.0f);
    Check(Gfx_BoundsVisible(Box(0,0,2,.2f),projection),"front geometry visible");
    Check(!Gfx_BoundsVisible(Box(0,0,-2,.2f),projection),"behind eye culled");
    Check(!Gfx_BoundsVisible(Box(20,0,2,.2f),projection),"right outside culled");
    Check(!Gfx_BoundsVisible(Box(0,-20,2,.2f),projection),"below outside culled");
    Check(!Gfx_BoundsVisible(Box(0,0,1200,1),projection),"far outside culled");
    Check(Gfx_BoundsVisible(Box(0,0,0,1),projection),"near-plane crossing retained");
    Check(Gfx_BoundsVisible(Box(1.7f,0,2,.5f),projection),"frustum-edge crossing retained");
    VR::Pose origin, eye; eye.orientation={0,1,0,0};
    const auto behind=VR::Multiply(VR::EyeFromGame(origin,eye,1),projection);
    Check(Gfx_BoundsVisible(Box(0,0,-2,.2f),behind),"head turn reveals rear geometry");
    Check(!Gfx_BoundsVisible(Box(0,0,2,.2f),behind),"head turn culls former front");
    GfxVertex vertices[2]{}; vertices[0].x=-2; vertices[0].y=-1; vertices[0].z=1;
    vertices[1].x=4; vertices[1].y=3; vertices[1].z=5;
    auto bounds=Gfx_VertexBounds(reinterpret_cast<const uint8_t*>(vertices),2);
    Check(bounds.valid && bounds.center[0]==1 && bounds.extent[0]==3 && bounds.center[2]==3,"bounds include every vertex");
    vertices[1].x=std::numeric_limits<float>::quiet_NaN();
    bounds=Gfx_VertexBounds(reinterpret_cast<const uint8_t*>(vertices),2);
    Check(!bounds.valid && Gfx_BoundsVisible(bounds,projection),"unknown bounds retained");
    // Eye positions must be tested separately at a stereo edge.
    eye={}; eye.position.x=-.032f;
    auto left=VR::Multiply(VR::EyeFromGame(origin,eye,1),projection);
    eye.position.x=.032f;
    auto right=VR::Multiply(VR::EyeFromGame(origin,eye,1),projection);
    auto edge=Box(std::tan(.7f)*2,0,2,.001f);
    Check(!Gfx_BoundsVisible(edge,left) && Gfx_BoundsVisible(edge,right),"per-eye edge visibility");
    std::puts("tracked culling: clip planes, head turns, stereo edges and conservative bounds pass");
}
