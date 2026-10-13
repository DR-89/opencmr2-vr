#include "render/draw_batch.h"
#include <cstdio>
#include <cstdlib>

static void Check(bool condition,const char *message) {
    if(!condition) { std::fprintf(stderr,"draw batching: %s\n",message); std::exit(1); }
}
static GfxCommand Draw(uint32_t vertex,uint32_t count,uint32_t index,uint32_t indices,uint32_t state=0) {
    GfxCommand c{}; c.type=GFX_CMD_DRAW; c.primitive=GFX_TRIANGLELIST;
    c.firstVertexByte=vertex*sizeof(GfxVertex); c.vertexCount=count;
    c.firstIndex=index; c.indexCount=indices; c.state=state; return c;
}
int main() {
    GfxFrame f;
    // Two independently indexed quads must retain the exact triangle order.
    f.indices={0,1,2,0,2,3,0,1,2,0,2,3};
    Gfx_AppendDraw(f,Draw(0,4,0,6)); Gfx_AppendDraw(f,Draw(4,4,6,6));
    Check(f.commands.size()==1 && f.commands[0].vertexCount==8 && f.commands[0].indexCount==12,"adjacent quads merge");
    Check(f.indices==std::vector<uint16_t>({0,1,2,0,2,3,4,5,6,4,6,7}),"index rebasing preserves triangle order");
    // A material change or render-target/clear command is a hard barrier.
    Gfx_AppendDraw(f,Draw(8,3,~0u,0,1)); Gfx_AppendDraw(f,Draw(11,3,~0u,0,2));
    Check(f.commands.size()==3,"material boundary");
    GfxCommand clear{}; clear.type=GFX_CMD_CLEAR; f.commands.push_back(clear);
    Gfx_AppendDraw(f,Draw(14,3,~0u,0,2));
    Check(f.commands.size()==5,"clear boundary");
    // Lists without indices merge, but strip topology must restart per draw.
    f={}; Gfx_AppendDraw(f,Draw(0,3,~0u,0)); Gfx_AppendDraw(f,Draw(3,3,~0u,0));
    Check(f.commands.size()==1 && f.commands[0].vertexCount==6,"plain triangle list");
    auto strip=Draw(6,3,~0u,0); strip.primitive=GFX_TRIANGLESTRIP;
    Gfx_AppendDraw(f,strip); strip.firstVertexByte+=3*sizeof(GfxVertex); Gfx_AppendDraw(f,strip);
    Check(f.commands.size()==3,"strip restart preserved");
    f={}; f.indices={0,1,2,0,1,2};
    Gfx_AppendDraw(f,Draw(0,65534,0,3)); Gfx_AppendDraw(f,Draw(65534,3,3,3));
    Check(f.commands.size()==2 && f.indices[3]==0,"16-bit overflow splits batch");
    f={}; f.indices={0,1,2,0,1,2};
    Gfx_AppendDraw(f,Draw(0,65533,0,3)); Gfx_AppendDraw(f,Draw(65533,3,3,3));
    Check(f.commands.size()==1 && f.indices[5]==65535,"largest valid index");
    f={}; f.indices={0,1,2,0,1,3};
    Gfx_AppendDraw(f,Draw(0,3,0,3)); Gfx_AppendDraw(f,Draw(3,3,3,3));
    Check(f.commands.size()==2,"invalid index is not rebased");
    f={}; Gfx_AppendDraw(f,Draw(0,4,~0u,0)); Gfx_AppendDraw(f,Draw(4,3,~0u,0));
    Check(f.commands.size()==2,"incomplete triangles keep draw boundary");
    std::puts("draw batching: topology, ordering, barriers and index limits pass");
}
