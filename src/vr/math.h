#pragma once
#include "port/gfx.h"
#include <cmath>
#include <algorithm>

namespace VR {
struct Vec3 { float x = 0, y = 0, z = 0; };
struct Quat { float x = 0, y = 0, z = 0, w = 1; };
struct Pose { Quat orientation; Vec3 position; };
struct Fov { float left, right, up, down; };
inline GfxMatrix Identity() { GfxMatrix m{}; m._11=m._22=m._33=m._44=1; return m; }
inline GfxMatrix Multiply(const GfxMatrix &a, const GfxMatrix &b) {
    GfxMatrix m{};
    for (int r=0;r<4;++r) for (int c=0;c<4;++c) for (int k=0;k<4;++k)
        (&m._11)[r*4+c] += (&a._11)[r*4+k]*(&b._11)[k*4+c];
    return m;
}
// Row-vector pose matrix in OpenXR's right-handed, -Z-forward coordinates.
inline GfxMatrix PoseMatrix(const Pose &p) {
    const auto q=p.orientation;
    GfxMatrix m=Identity();
    m._11=1-2*(q.y*q.y+q.z*q.z); m._12=2*(q.x*q.y+q.z*q.w); m._13=2*(q.x*q.z-q.y*q.w);
    m._21=2*(q.x*q.y-q.z*q.w); m._22=1-2*(q.x*q.x+q.z*q.z); m._23=2*(q.y*q.z+q.x*q.w);
    m._31=2*(q.x*q.z+q.y*q.w); m._32=2*(q.y*q.z-q.x*q.w); m._33=1-2*(q.x*q.x+q.y*q.y);
    m._41=p.position.x; m._42=p.position.y; m._43=p.position.z;
    return m;
}
inline GfxMatrix InverseRigid(const GfxMatrix &a) {
    auto m=Identity();
    for(int r=0;r<3;++r) for(int c=0;c<3;++c) (&m._11)[r*4+c]=(&a._11)[c*4+r];
    for(int c=0;c<3;++c) (&m._41)[c]=-(a._41*(&m._11)[c]+a._42*(&m._11)[4+c]+a._43*(&m._11)[8+c]);
    return m;
}
// Game view coordinates are +Z forward. Metres -> game units only affects translation.
inline GfxMatrix EyeFromGame(const Pose &origin, const Pose &eye, float unitsPerMetre) {
    auto m=Multiply(PoseMatrix(origin),InverseRigid(PoseMatrix(eye)));
    const float sign[4]={1,1,-1,1};
    for(int r=0;r<4;++r) for(int c=0;c<4;++c) (&m._11)[r*4+c]*=sign[r]*sign[c];
    m._41*=unitsPerMetre; m._42*=unitsPerMetre; m._43*=unitsPerMetre;
    return m;
}
// SDL clip convention: Y up, depth 0..1. SDL handles Vulkan's Y inversion.
inline GfxMatrix Projection(const Fov &f, float nearZ, float farZ) {
    const float l=std::tan(f.left), r=std::tan(f.right), u=std::tan(f.up), d=std::tan(f.down);
    GfxMatrix m{};
    m._11=2/(r-l); m._22=2/(u-d);
    m._31=-(r+l)/(r-l); m._32=-(u+d)/(u-d);
    m._33=farZ/(farZ-nearZ); m._34=1; m._43=-nearZ*m._33;
    return m;
}
inline Pose SeatedOrigin(const Pose &head) {
    // Recenter position and heading, preserving the world's vertical.
    auto m=PoseMatrix(head);
    float yaw=std::atan2(m._31,m._33);
    return {{0,std::sin(yaw/2),0,std::cos(yaw/2)},head.position};
}
inline float Deadzone(float value, float zone=0.15f) {
    if (!std::isfinite(value)) return 0;
    value=std::clamp(value,-1.0f,1.0f);
    return std::copysign(std::max(0.0f,(std::fabs(value)-zone)/(1-zone)),value);
}
}
