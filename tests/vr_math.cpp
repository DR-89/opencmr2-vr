#include "vr/math.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
static void Near(float a,float b,const char *label) {
    if(!std::isfinite(a) || std::fabs(a-b)>0.0001f) { std::fprintf(stderr,"%s: %f != %f\n",label,a,b); std::exit(1); }
}
static VR::Vec3 Point(VR::Vec3 v,const GfxMatrix &m) {
    float w=v.x*m._14+v.y*m._24+v.z*m._34+m._44;
    return {(v.x*m._11+v.y*m._21+v.z*m._31+m._41)/w,
            (v.x*m._12+v.y*m._22+v.z*m._32+m._42)/w,
            (v.x*m._13+v.y*m._23+v.z*m._33+m._43)/w};
}
int main() {
    const VR::Fov f{-0.7f,0.9f,0.8f,-0.6f}; auto p=VR::Projection(f,0.05f,1000);
    Near(Point({std::tan(f.left),0,1},p).x,-1,"left edge");
    Near(Point({std::tan(f.right),0,1},p).x,1,"right edge");
    Near(Point({0,std::tan(f.up),1},p).y,1,"top edge");
    Near(Point({0,std::tan(f.down),1},p).y,-1,"bottom edge");
    Near(Point({0,0,0.05f},p).z,0,"near depth"); Near(Point({0,0,1000},p).z,1,"far depth");
    VR::Pose origin{},left{},right{}; left.position.x=-0.032f; right.position.x=0.032f;
    Near(VR::EyeFromGame(origin,left,1)._41,0.032f,"left eye");
    Near(VR::EyeFromGame(origin,right,1)._41,-0.032f,"right eye");
    right.position.z=-0.2f; Near(VR::EyeFromGame(origin,right,1)._43,-0.2f,"lean forward");
    Near(VR::EyeFromGame(origin,right,2)._43,-0.4f,"world scale");
    VR::Pose yaw{{0,std::sin(0.4f),0,std::cos(0.4f)},{3,1.7f,-2}};
    auto inverse=VR::Multiply(VR::PoseMatrix(yaw),VR::InverseRigid(VR::PoseMatrix(yaw)));
    auto identity=VR::Identity(); for(int i=0;i<16;++i) Near((&inverse._11)[i],(&identity._11)[i],"inverse pose");
    auto recentered=VR::EyeFromGame(VR::SeatedOrigin(yaw),yaw,1);
    for(int i=0;i<16;++i) Near((&recentered._11)[i],(&identity._11)[i],"seated recenter");
    Near(VR::Deadzone(0.1f),0,"deadzone"); Near(VR::Deadzone(-1),-1,"full left");
    Near(VR::Deadzone(1),1,"full right"); Near(VR::Deadzone(2),1,"clamp");
    Near(VR::Deadzone(std::numeric_limits<float>::quiet_NaN()),0,"invalid axis");
    std::puts("VR projection, IPD, pose, scale, recenter and deadzone tests passed.");
}
