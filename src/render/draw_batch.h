#pragma once
#include "render/gfx_internal.h"

// PORT: merge adjacent triangle lists without reordering transparent geometry.
// Vertex/index data is already appended; each draw's indices are local to it.
inline void Gfx_AppendDraw(GfxFrame &frame, const GfxCommand &draw)
{
    if (!frame.commands.empty()) {
        auto &last=frame.commands.back();
        const auto stride=draw.transformed ? sizeof(GfxTLVertex) : sizeof(GfxVertex);
        const bool indexed=draw.firstIndex!=~0u;
        if (last.type==GFX_CMD_DRAW && draw.primitive==GFX_TRIANGLELIST &&
            last.primitive==draw.primitive && last.transformed==draw.transformed && last.state==draw.state &&
            uint64_t(last.firstVertexByte)+uint64_t(last.vertexCount)*stride==draw.firstVertexByte &&
            (last.firstIndex!=~0u)==indexed) {
            if (!indexed && last.vertexCount%3==0 && draw.vertexCount%3==0) {
                last.vertexCount+=draw.vertexCount;
                return;
            }
            if (indexed && last.indexCount%3==0 && draw.indexCount%3==0 &&
                uint64_t(last.firstIndex)+last.indexCount==draw.firstIndex &&
                uint64_t(last.vertexCount)+draw.vertexCount<=65536 &&
                uint64_t(draw.firstIndex)+draw.indexCount<=frame.indices.size()) {
                bool valid=true;
                for (uint32_t i=0;i<draw.indexCount;++i)
                    if (frame.indices[draw.firstIndex+i]>=draw.vertexCount) { valid=false; break; }
                if (valid) {
                    for(uint32_t i=0;i<draw.indexCount;++i)
                        frame.indices[draw.firstIndex+i]+=static_cast<uint16_t>(last.vertexCount);
                    last.vertexCount+=draw.vertexCount;
                    last.indexCount+=draw.indexCount;
                    return;
                }
            }
        }
    }
    frame.commands.push_back(draw);
}
