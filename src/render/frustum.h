#pragma once
#include "port/gfx.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>

struct GfxBounds {
    float center[3]{}, extent[3]{};
    bool valid=false;
};
inline GfxBounds Gfx_VertexBounds(const uint8_t *data,uint32_t count) {
    GfxBounds bounds;
    if(!count) return bounds;
    float minimum[3],maximum[3];
    std::memcpy(minimum,data,sizeof(minimum)); std::memcpy(maximum,data,sizeof(maximum));
    for(uint32_t i=0;i<count;++i) {
        float point[3]; std::memcpy(point,data+size_t(i)*sizeof(GfxVertex),sizeof(point));
        for(int axis=0;axis<3;++axis) {
            if(!std::isfinite(point[axis])) return bounds;
            minimum[axis]=std::min(minimum[axis],point[axis]);
            maximum[axis]=std::max(maximum[axis],point[axis]);
        }
    }
    for(int axis=0;axis<3;++axis) {
        bounds.center[axis]=minimum[axis]*0.5f+maximum[axis]*0.5f;
        bounds.extent[axis]=maximum[axis]*0.5f-minimum[axis]*0.5f;
    }
    bounds.valid=true;
    return bounds;
}
// Conservative object-space box test against row-vector clip planes. A box is
// discarded only when it lies entirely outside one plane of this tracked eye.
inline bool Gfx_BoundsVisible(const GfxBounds &box,const GfxMatrix &clip) {
    if(!box.valid) return true;
    const float *m=&clip._11;
    for(int plane=0;plane<6;++plane) {
        const int column=plane/2;
        const float sign=(plane&1) ? -1.0f : 1.0f;
        float p[4];
        for(int row=0;row<4;++row)
            p[row]=(plane==4 ? 0.0f : m[row*4+3])+sign*m[row*4+column];
        float distance=p[3],radius=0;
        for(int axis=0;axis<3;++axis) {
            distance+=p[axis]*box.center[axis];
            radius+=std::fabs(p[axis])*box.extent[axis];
        }
        if(std::isfinite(distance) && std::isfinite(radius) &&
           distance+radius < -0.0001f*(1.0f+std::fabs(distance)+radius)) return false;
    }
    return true;
}
