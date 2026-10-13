// The renderer's front end: the gfx.h state machine, CPU copies of textures
// and vertex buffers, and the recording of each frame for the executor.

#include "platform/platform.h"
#include "port/sys.h"
#include "port/diagnostics.h"
#include "render/gfx_internal.h"
#include "render/draw_batch.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

namespace {

GfxExecutor *s_executor;
GfxDrawState s_state;
bool s_stateDirty = true;
GfxFrame s_frame;
GfxReadback s_readback;
std::vector<GfxTexture *> s_textures;     // every live texture (not face views)
int s_width = 640, s_height = 480;
GfxTexture *s_target;
std::vector<std::pair<GfxDrawState, GfxTexture *>> s_savedStates;
std::vector<std::pair<int, int>> s_modes;

GfxMatrix Identity()
{
    GfxMatrix m;
    memset(&m, 0, sizeof(m));
    m._11 = m._22 = m._33 = m._44 = 1.0f;
    return m;
}

void ResetState()
{
    s_state = GfxDrawState();
    s_state.world = s_state.view = s_state.projection = Identity();
    s_state.texture[0] = s_state.texture[1] = Identity();
    memset(s_state.lights, 0, sizeof(s_state.lights));
    memset(&s_state.material, 0, sizeof(s_state.material));
    s_state.stages[1].colorOp = GFX_TOP_DISABLE;
    s_state.stages[1].alphaOp = GFX_TOP_DISABLE;
    s_state.stages[1].texCoordIndex = 1;
    s_state.viewport.x = s_state.viewport.y = 0;
    s_state.viewport.width = s_width;
    s_state.viewport.height = s_height;
    s_state.viewport.minZ = 0.0f;
    s_state.viewport.maxZ = 1.0f;
    s_stateDirty = true;
}

inline void Touch()
{
    s_stateDirty = true;
}

uint32_t RecordState()
{
    if (s_stateDirty || s_frame.states.empty()) {
        // Byte comparison is conservative: different padding only misses a
        // merge; identical snapshots are safe to share across adjacent draws.
        if (s_frame.states.empty() || memcmp(&s_frame.states.back(), &s_state, sizeof(s_state)) != 0)
            s_frame.states.push_back(s_state);
        s_stateDirty = false;
    }
    return (uint32_t)s_frame.states.size() - 1;
}

uint32_t AppendVertices(const void *data, size_t bytes)
{
    size_t offset = s_frame.vertices.size();
    s_frame.vertices.resize(offset + bytes);
    memcpy(s_frame.vertices.data() + offset, data, bytes);
    return (uint32_t)offset;
}

// OPENCMR2_GFX_TRACE=file: one line per draw (primitive, counts, a hash of the
// vertices, indices and bound texture sizes, and the calling game function),
// "F n" per presented frame. Builds for different targets must write the same
// lines; diffing them finds drawing that differs (tools: addr2line on caller).
static FILE *TraceFile()
{
    static FILE *file = getenv("OPENCMR2_GFX_TRACE") ? fopen(getenv("OPENCMR2_GFX_TRACE"), "w") : NULL;
    return file;
}
static const void *s_traceCaller;
static unsigned s_traceFrame;

static uint32_t TraceHash(uint32_t h, const void *data, size_t size)
{
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < size; i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

// Records a draw; fans become lists (SDL_GPU has no fans).
void RecordDraw(int primitive, bool transformed, const void *vertices, DWORD vertexCount, const WORD *indices,
                DWORD indexCount)
{
    if (FILE *trace = TraceFile()) {
        uint32_t h = TraceHash(2166136261u, vertices, vertexCount * (transformed ? sizeof(GfxTLVertex) : sizeof(GfxVertex)));
        if (indices != NULL)
            h = TraceHash(h, indices, indexCount * sizeof(WORD));
        for (int k = 0; k < 2; k++) {
            const GfxTexture *t = s_state.stages[k].texture;
            int size[2] = {t ? t->width : 0, t ? t->height : 0};
            h = TraceHash(h, size, sizeof(size));
        }
        fprintf(trace, "%d %d %u %u %08x %p\n", primitive, transformed ? 1 : 0, (unsigned)vertexCount,
                (unsigned)indexCount, h, s_traceCaller);
        static int dumpFrame = getenv("OPENCMR2_GFX_TRACE_FRAME") ? atoi(getenv("OPENCMR2_GFX_TRACE_FRAME")) : -1;
        if ((int)s_traceFrame == dumpFrame && transformed)
            for (DWORD v = 0; v < vertexCount; v++) {
                const GfxTLVertex *t = (const GfxTLVertex *)vertices + v;
                fprintf(trace, "  v %g %g %g %g c=%08x s=%08x uv=%g,%g tex=%dx%d\n", t->sx, t->sy, t->sz, t->rhw, t->color, t->specular, t->tu, t->tv,
                        s_state.stages[0].texture ? s_state.stages[0].texture->width : 0, s_state.stages[0].texture ? s_state.stages[0].texture->height : 0);
            }
    }
    if (vertexCount == 0 || s_executor == NULL)
        return;
    std::vector<uint16_t> fan;
    if (primitive == GFX_TRIANGLEFAN) {
        DWORD n = indices ? indexCount : vertexCount;
        for (DWORD i = 1; i + 1 < n; i++) {
            fan.push_back(indices ? indices[0] : 0);
            fan.push_back(indices ? indices[i] : (uint16_t)i);
            fan.push_back(indices ? indices[i + 1] : (uint16_t)(i + 1));
        }
        primitive = GFX_TRIANGLELIST;
        indices = fan.data();
        indexCount = (DWORD)fan.size();
        if (indexCount == 0)
            return;
    }

    GfxCommand cmd = {};
    cmd.type = GFX_CMD_DRAW;
    cmd.primitive = primitive;
    cmd.transformed = transformed;
    cmd.vertexCount = vertexCount;
    cmd.firstVertexByte = AppendVertices(vertices, vertexCount * (transformed ? sizeof(GfxTLVertex) : sizeof(GfxVertex)));
    if (indices != NULL) {
        cmd.firstIndex = (uint32_t)s_frame.indices.size();
        cmd.indexCount = indexCount;
        s_frame.indices.insert(s_frame.indices.end(), indices, indices + indexCount);
    } else {
        cmd.firstIndex = ~0u;
        cmd.indexCount = 0;
    }
    cmd.state = RecordState();
    Gfx_AppendDraw(s_frame, cmd);
}

GfxStageState *Stage(int stage)
{
    return stage >= 0 && stage < 2 ? &s_state.stages[stage] : NULL;
}

GfxTexture *Root(GfxTexture *t)
{
    return t != NULL && t->cube != NULL ? t->cube : t;
}

void BuildModes()
{
    static const int candidates[][2] = { { 640, 480 }, { 800, 600 }, { 1024, 768 }, { 1280, 720 }, { 1280, 960 },
                                         { 1600, 900 }, { 1920, 1080 }, { 2560, 1440 }, { 3840, 2160 } };
    int desktopW, desktopH;

    s_modes.clear();
    Sys_GetDesktopSize(&desktopW, &desktopH);
    for (const auto &c : candidates)
        if (c[0] <= desktopW && c[1] <= desktopH)
            s_modes.push_back({ c[0], c[1] });
    if (std::find(s_modes.begin(), s_modes.end(), std::make_pair(desktopW, desktopH)) == s_modes.end())
        s_modes.push_back({ desktopW, desktopH });
    std::sort(s_modes.begin(), s_modes.end());
    while (s_modes.size() > 10)
        s_modes.erase(s_modes.begin() + 1);
}

} // namespace

// ---- formats -------------------------------------------------------------------------

int Gfx_BytesPerPixel(int format)
{
    switch (format) {
    case GFX_FORMAT_BGRX8:
    case GFX_FORMAT_BGRA8:
        return 4;
    case GFX_FORMAT_BUMP_DUDVL:
        return 3;
    default:
        return 0;
    }
}

size_t Gfx_LevelSize(int format, int width, int height, LONG *pitch)
{
    if (format == GFX_FORMAT_BC1 || format == GFX_FORMAT_BC3) {
        int blockBytes = format == GFX_FORMAT_BC1 ? 8 : 16;
        int bw = (width + 3) / 4, bh = (height + 3) / 4;
        if (pitch)
            *pitch = bw * blockBytes;
        return (size_t)bw * bh * blockBytes;
    }
    int bpp = Gfx_BytesPerPixel(format);
    if (pitch)
        *pitch = width * bpp;
    return (size_t)width * height * bpp;
}

extern "C" void Gfx_GetPixelFormat(int format, GfxPixelFormat *out)
{
    memset(out, 0, sizeof(*out));
    switch (format) {
    case GFX_FORMAT_BGRX8:
        out->flags = GFX_PF_RGB;
        out->bitsPerPixel = 32;
        out->redMask = 0x00ff0000;
        out->greenMask = 0x0000ff00;
        out->blueMask = 0x000000ff;
        break;
    case GFX_FORMAT_BGRA8:
        out->flags = GFX_PF_RGB | GFX_PF_ALPHA;
        out->bitsPerPixel = 32;
        out->redMask = 0x00ff0000;
        out->greenMask = 0x0000ff00;
        out->blueMask = 0x000000ff;
        out->alphaMask = 0xff000000;
        break;
    case GFX_FORMAT_BC1:
        out->flags = GFX_PF_FOURCC;
        out->fourCC = MAKEFOURCC('D', 'X', 'T', '1');
        break;
    case GFX_FORMAT_BC3:
        out->flags = GFX_PF_FOURCC;
        out->fourCC = MAKEFOURCC('D', 'X', 'T', '5');
        break;
    case GFX_FORMAT_BUMP_DUDVL:
        out->flags = GFX_PF_BUMP;
        out->bitsPerPixel = 24;
        out->redMask = 0x0000ff;
        out->greenMask = 0x00ff00;
        out->blueMask = 0xff0000;
        break;
    }
}

// ---- device and frame ----------------------------------------------------------------

extern "C" BOOL Gfx_SetVideoMode(int width, int height, BOOL fullscreen)
{
    if (s_executor == NULL) {
        const char *headless = SDL_getenv("OPENCMR2_HEADLESS");
        if ((headless != NULL && *headless != '0') || Platform_GetSettingInt("video.headless", 0))
            s_executor = Gfx_CreateNullExecutor();
        else
            s_executor = Gfx_CreateGpuExecutor();
    }
    s_width = width;
    s_height = height;
    if (!s_executor->SetVideoMode(width, height, fullscreen != 0)) {
        Sys_Log("gfx: could not set %dx%d", width, height);
        return FALSE;
    }
    ResetState();
    return TRUE;
}

extern "C" void Gfx_SetFullscreen(BOOL fullscreen)
{
    if (s_executor != NULL)
        s_executor->SetFullscreen(fullscreen != 0);
}

extern "C" void Gfx_Shutdown(void)
{
    while (!s_textures.empty())
        Gfx_DestroyTexture(s_textures.back());
    delete s_executor;
    s_executor = NULL;
}

extern "C" int Gfx_GetDisplayModeCount(void)
{
    if (s_modes.empty())
        BuildModes();
    return (int)s_modes.size();
}

extern "C" void Gfx_GetDisplayMode(int index, int *width, int *height)
{
    if (s_modes.empty())
        BuildModes();
    *width = s_modes[index].first;
    *height = s_modes[index].second;
}

extern "C" void Gfx_BeginScene(void)
{
}

extern "C" void Gfx_EndScene(void)
{
}

static void Present(BOOL vsync, bool stretch)
{
    if (FILE *trace = TraceFile())
        fprintf(trace, "F %u\n", s_traceFrame++);
    if (Diagnostics::observer)
        Diagnostics::observer->BeginPresent();
    if (s_executor != NULL)
        s_executor->Execute(s_frame, s_textures, s_readback, true, vsync != 0, stretch);
    s_frame.Reset();
    s_stateDirty = true;
    // A frame always starts on the back buffer.
    s_target = NULL;
    if (Diagnostics::observer)
        Diagnostics::observer->EndPresent();
}

extern "C" void Gfx_Present(BOOL vsync) { Present(vsync, false); }
extern "C" void Gfx_PresentStretched(BOOL vsync) { Present(vsync, true); }

// Runs what is recorded so far without ending the frame.
static void Flush(void)
{
    if (s_executor != NULL)
        s_executor->Execute(s_frame, s_textures, s_readback, false, false);
    s_frame.Reset();
    s_stateDirty = true;
    if (s_target != NULL)
        Gfx_SetRenderTarget(s_target);
}

extern "C" void Gfx_Clear(DWORD flags, DWORD color, float z)
{
    GfxCommand cmd = {};
    cmd.type = GFX_CMD_CLEAR;
    cmd.clearFlags = flags;
    cmd.clearColor = color;
    cmd.clearZ = z;
    cmd.clearViewport = s_state.viewport;
    s_frame.commands.push_back(cmd);
}

extern "C" void Gfx_SetViewport(const GfxViewport *viewport)
{
    s_state.viewport = *viewport;
    Touch();
}

extern "C" void Gfx_GetViewport(GfxViewport *viewport)
{
    *viewport = s_state.viewport;
}

extern "C" void Gfx_PushState(void)
{
    s_savedStates.push_back({s_state, s_target});
}

extern "C" void Gfx_PopState(void)
{
    if (s_savedStates.empty())
        return;
    const auto saved = s_savedStates.back();
    s_savedStates.pop_back();
    s_state = saved.first;
    Touch();
    if (s_target != saved.second)
        Gfx_SetRenderTarget(saved.second);
}

extern "C" void Gfx_SetRenderTarget(GfxTexture *target)
{
    GfxCommand cmd = {};
    cmd.type = GFX_CMD_TARGET;
    cmd.target = target;
    s_frame.commands.push_back(cmd);
    s_target = target;
    if (target != NULL)
        Root(target)->renderedTo = true;
}

// ---- fixed-function state --------------------------------------------------------------

extern "C" void Gfx_SetTransform(int type, const GfxMatrix *matrix)
{
    switch (type) {
    case GFX_TRANSFORM_WORLD: s_state.world = *matrix; break;
    case GFX_TRANSFORM_VIEW: s_state.view = *matrix; break;
    case GFX_TRANSFORM_PROJECTION: s_state.projection = *matrix; break;
    case GFX_TRANSFORM_TEXTURE0: s_state.texture[0] = *matrix; break;
    case GFX_TRANSFORM_TEXTURE1: s_state.texture[1] = *matrix; break;
    }
    Touch();
}

extern "C" void Gfx_GetTransform(int type, GfxMatrix *matrix)
{
    switch (type) {
    case GFX_TRANSFORM_WORLD: *matrix = s_state.world; break;
    case GFX_TRANSFORM_VIEW: *matrix = s_state.view; break;
    case GFX_TRANSFORM_PROJECTION: *matrix = s_state.projection; break;
    case GFX_TRANSFORM_TEXTURE0: *matrix = s_state.texture[0]; break;
    case GFX_TRANSFORM_TEXTURE1: *matrix = s_state.texture[1]; break;
    default: *matrix = Identity(); break;
    }
}

extern "C" void Gfx_SetLight(int index, const GfxLight *light)
{
    if (index >= 0 && index < 8) {
        s_state.lights[index] = *light;
        Touch();
    }
}

extern "C" void Gfx_GetLight(int index, GfxLight *light)
{
    if (index >= 0 && index < 8)
        *light = s_state.lights[index];
}

extern "C" void Gfx_EnableLight(int index, BOOL enable)
{
    if (index >= 0 && index < 8) {
        s_state.lightEnabled[index] = enable != 0;
        Touch();
    }
}

extern "C" void Gfx_SetMaterial(const GfxMaterial *material)
{
    s_state.material = *material;
    Touch();
}

#define SETTER(name, type, field)       \
    extern "C" void name(type value)    \
    {                                   \
        if (s_state.field != value) { s_state.field = value; Touch(); } \
    }

SETTER(Gfx_SetAmbient, DWORD, ambient)
SETTER(Gfx_SetDiffuseMaterialSource, int, diffuseSource)
SETTER(Gfx_SetSpecularMaterialSource, int, specularSource)
SETTER(Gfx_SetAmbientMaterialSource, int, ambientSource)
SETTER(Gfx_SetEmissiveMaterialSource, int, emissiveSource)
SETTER(Gfx_SetFogColor, DWORD, fogColor)
SETTER(Gfx_SetFogVertexMode, int, fogVertexMode)
SETTER(Gfx_SetFogTableMode, int, fogTableMode)
SETTER(Gfx_SetFogStart, float, fogStart)
SETTER(Gfx_SetFogEnd, float, fogEnd)
SETTER(Gfx_SetDepthFunc, int, depthFunc)
SETTER(Gfx_SetCullMode, int, cull)
SETTER(Gfx_SetSrcBlend, int, srcBlend)
SETTER(Gfx_SetDestBlend, int, destBlend)
SETTER(Gfx_SetAlphaFunc, int, alphaFunc)
SETTER(Gfx_SetAlphaRef, DWORD, alphaRef)
SETTER(Gfx_SetTextureFactor, DWORD, textureFactor)

#define BOOL_SETTER(name, field)        \
    extern "C" void name(BOOL value)    \
    {                                   \
        if (s_state.field != (value != 0)) { s_state.field = value != 0; Touch(); } \
    }

BOOL_SETTER(Gfx_SetLighting, lighting)
BOOL_SETTER(Gfx_SetVRWorld, vrWorld)
BOOL_SETTER(Gfx_SetSpecular, specular)
BOOL_SETTER(Gfx_SetLocalViewer, localViewer)
BOOL_SETTER(Gfx_SetNormalizeNormals, normalizeNormals)
BOOL_SETTER(Gfx_SetColorVertex, colorVertex)
BOOL_SETTER(Gfx_SetFog, fog)
BOOL_SETTER(Gfx_SetDepthTest, depthTest)
BOOL_SETTER(Gfx_SetDepthWrite, depthWrite)
BOOL_SETTER(Gfx_SetAlphaBlend, alphaBlend)
BOOL_SETTER(Gfx_SetAlphaTest, alphaTest)

#define STAGE_SETTER(name, type, field)             \
    extern "C" void name(int stage, type value)     \
    {                                               \
        GfxStageState *s = Stage(stage);            \
        if (s != NULL && s->field != value) {        \
            s->field = value;                       \
            Touch();                                \
        }                                           \
    }

STAGE_SETTER(Gfx_SetTexture, GfxTexture *, texture)
STAGE_SETTER(Gfx_SetStageColorOp, int, colorOp)
STAGE_SETTER(Gfx_SetStageColorArg1, int, colorArg1)
STAGE_SETTER(Gfx_SetStageColorArg2, int, colorArg2)
STAGE_SETTER(Gfx_SetStageAlphaOp, int, alphaOp)
STAGE_SETTER(Gfx_SetStageAlphaArg1, int, alphaArg1)
STAGE_SETTER(Gfx_SetStageAlphaArg2, int, alphaArg2)
STAGE_SETTER(Gfx_SetStageAddress, int, address)
STAGE_SETTER(Gfx_SetStageMinFilter, int, minFilter)
STAGE_SETTER(Gfx_SetStageMagFilter, int, magFilter)
STAGE_SETTER(Gfx_SetStageMipFilter, int, mipFilter)
STAGE_SETTER(Gfx_SetStageMaxMipLevel, int, maxMipLevel)
STAGE_SETTER(Gfx_SetStageTexCoordIndex, DWORD, texCoordIndex)
STAGE_SETTER(Gfx_SetStageTextureTransform, int, textureTransform)
STAGE_SETTER(Gfx_SetStageBumpEnvLScale, float, bumpLumScale)
STAGE_SETTER(Gfx_SetStageBumpEnvLOffset, float, bumpLumOffset)

extern "C" void Gfx_SetStageBumpEnvMat(int stage, int index, float value)
{
    GfxStageState *s = Stage(stage);
    if (s != NULL && index >= 0 && index < 4) {
        s->bumpMat[index] = value;
        Touch();
    }
}

// ---- geometry ----------------------------------------------------------------------

extern "C" GfxVertexBuffer *Gfx_CreateVertexBuffer(DWORD vertexCount)
{
    GfxVertexBuffer *buffer = new GfxVertexBuffer;
    buffer->vertices.resize(vertexCount);
    memset(buffer->vertices.data(), 0, vertexCount * sizeof(GfxVertex));
    return buffer;
}

extern "C" void Gfx_DestroyVertexBuffer(GfxVertexBuffer *buffer)
{
    delete buffer;
}

extern "C" GfxVertex *Gfx_LockVertexBuffer(GfxVertexBuffer *buffer)
{
    return buffer != NULL ? buffer->vertices.data() : NULL;
}

extern "C" void Gfx_UnlockVertexBuffer(GfxVertexBuffer *)
{
}

extern "C" void Gfx_DrawPrimitiveVB(int primitive, GfxVertexBuffer *buffer, DWORD firstVertex, DWORD vertexCount)
{
    s_traceCaller = __builtin_return_address(0);
    if (buffer == NULL || firstVertex + vertexCount > buffer->vertices.size())
        return;
    RecordDraw(primitive, false, buffer->vertices.data() + firstVertex, vertexCount, NULL, 0);
}

extern "C" void Gfx_DrawIndexedPrimitiveVB(int primitive, GfxVertexBuffer *buffer, DWORD firstVertex, DWORD vertexCount,
                                           const WORD *indices, DWORD indexCount)
{
    s_traceCaller = __builtin_return_address(0);
    if (buffer == NULL || firstVertex + vertexCount > buffer->vertices.size())
        return;
    RecordDraw(primitive, false, buffer->vertices.data() + firstVertex, vertexCount, indices, indexCount);
}

extern "C" void Gfx_DrawPrimitive(int primitive, const GfxVertex *vertices, DWORD vertexCount)
{
    s_traceCaller = __builtin_return_address(0);
    RecordDraw(primitive, false, vertices, vertexCount, NULL, 0);
}

extern "C" void Gfx_DrawIndexedPrimitive(int primitive, const GfxVertex *vertices, DWORD vertexCount, const WORD *indices,
                                         DWORD indexCount)
{
    s_traceCaller = __builtin_return_address(0);
    RecordDraw(primitive, false, vertices, vertexCount, indices, indexCount);
}

extern "C" void Gfx_DrawPrimitiveTL(int primitive, const GfxTLVertex *vertices, DWORD vertexCount)
{
    s_traceCaller = __builtin_return_address(0);
    RecordDraw(primitive, true, vertices, vertexCount, NULL, 0);
}

extern "C" void Gfx_DrawIndexedPrimitiveTL(int primitive, const GfxTLVertex *vertices, DWORD vertexCount,
                                           const WORD *indices, DWORD indexCount)
{
    s_traceCaller = __builtin_return_address(0);
    RecordDraw(primitive, true, vertices, vertexCount, indices, indexCount);
}

// ---- textures -------------------------------------------------------------------------

extern "C" GfxTexture *Gfx_CreateTexture(int width, int height, int format, int mipLevels, DWORD flags)
{
    if (width <= 0 || height <= 0)
        return NULL;
    // Compressed and bump textures keep one level: the original's mip
    // generation (a DirectDraw stretch blit) only worked on plain RGB.
    if (format != GFX_FORMAT_BGRX8 && format != GFX_FORMAT_BGRA8)
        mipLevels = 1;
    if (flags & (GFX_TEXTURE_RENDER_TARGET | GFX_TEXTURE_CUBE))
        mipLevels = 1;
    if (mipLevels < 1)
        mipLevels = 1;
    if (mipLevels > 16)
        mipLevels = 16;

    GfxTexture *t = new GfxTexture();
    t->width = width;
    t->height = height;
    t->format = format;
    t->mipLevels = mipLevels;
    t->flags = flags;
    t->cube = NULL;
    t->face = 0;
    t->dirty = true;
    t->renderedTo = false;
    t->gpu = NULL;
    int faces = (flags & GFX_TEXTURE_CUBE) ? 6 : 1;
    for (int f = 0; f < faces; f++) {
        for (int m = 0; m < mipLevels; m++) {
            int w = std::max(1, width >> m), h = std::max(1, height >> m);
            t->levels[f][m].assign(Gfx_LevelSize(format, w, h, NULL), 0);
        }
    }
    for (int f = 0; f < 6; f++)
        t->faceViews[f] = NULL;
    if (flags & GFX_TEXTURE_CUBE) {
        t->faceViews[0] = t;
        for (int f = 1; f < 6; f++) {
            GfxTexture *v = new GfxTexture();
            v->width = width;
            v->height = height;
            v->format = format;
            v->mipLevels = 1;
            v->flags = flags;
            v->cube = t;
            v->face = f;
            v->gpu = NULL;
            t->faceViews[f] = v;
        }
    }
    s_textures.push_back(t);
    return t;
}

extern "C" void Gfx_DestroyTexture(GfxTexture *texture)
{
    if (texture == NULL || texture->cube != NULL)
        return;     // face views go with their cube
    for (int i = 0; i < 2; i++)
        if (Root(s_state.stages[i].texture) == texture)
            s_state.stages[i].texture = NULL;
    // Draws recorded this frame may still use it: flush them first.
    for (const GfxDrawState &state : s_frame.states)
        for (const GfxStageState &stage : state.stages)
            if (Root(stage.texture) == texture) {
                Flush();
                goto flushed;
            }
flushed:
    if (s_executor != NULL)
        s_executor->DestroyTexture(texture);
    s_textures.erase(std::remove(s_textures.begin(), s_textures.end(), texture), s_textures.end());
    for (int f = 1; f < 6; f++)
        delete texture->faceViews[f];
    delete texture;
}

extern "C" int Gfx_GetTextureMipCount(GfxTexture *texture)
{
    return texture != NULL ? Root(texture)->mipLevels : 0;
}

extern "C" GfxTexture *Gfx_GetCubeFace(GfxTexture *cube, int face)
{
    if (cube == NULL || face < 0 || face > 5)
        return NULL;
    cube = Root(cube);
    return cube->faceViews[face] != NULL ? cube->faceViews[face] : cube;
}

extern "C" BOOL Gfx_LockTexture(GfxTexture *texture, int face, int mip, GfxLockedRect *out)
{
    if (texture == NULL)
        return FALSE;
    if (texture->cube != NULL) {
        face = texture->face;
        texture = texture->cube;
    }
    if (mip < 0 || mip >= texture->mipLevels || face < 0 || face > 5 || texture->levels[face][mip].empty())
        return FALSE;
    if (texture->renderedTo && s_executor != NULL) {
        s_executor->ReadTexture(texture);
        texture->renderedTo = false;
    }
    int w = std::max(1, texture->width >> mip), h = std::max(1, texture->height >> mip);
    memset(out, 0, sizeof(*out));
    out->pixels = texture->levels[face][mip].data();
    Gfx_LevelSize(texture->format, w, h, &out->pitch);
    out->width = w;
    out->height = h;
    Gfx_GetPixelFormat(texture->format, &out->format);
    return TRUE;
}

extern "C" void Gfx_UnlockTexture(GfxTexture *texture, int, int)
{
    if (texture != NULL)
        Root(texture)->dirty = true;
}

namespace {

// The top level of a texture as BGRA pixels.
std::vector<uint32_t> ToBGRA(GfxTexture *t)
{
    std::vector<uint32_t> out((size_t)t->width * t->height);
    const uint8_t *src = t->levels[0][0].data();
    switch (t->format) {
    case GFX_FORMAT_BGRX8:
        for (size_t i = 0; i < out.size(); i++)
            out[i] = ((const uint32_t *)src)[i] | 0xff000000;
        break;
    case GFX_FORMAT_BGRA8:
        memcpy(out.data(), src, out.size() * 4);
        break;
    case GFX_FORMAT_BUMP_DUDVL:
        for (size_t i = 0; i < out.size(); i++)
            out[i] = src[i * 3] | src[i * 3 + 1] << 8 | src[i * 3 + 2] << 16 | 0xff000000;
        break;
    case GFX_FORMAT_BC1:
    case GFX_FORMAT_BC3: {
        int blockBytes = t->format == GFX_FORMAT_BC1 ? 8 : 16;
        int bw = (t->width + 3) / 4, bh = (t->height + 3) / 4;
        uint32_t block[16];
        for (int by = 0; by < bh; by++) {
            for (int bx = 0; bx < bw; bx++) {
                Gfx_DecodeBlock(t->format, src + (by * bw + bx) * blockBytes, block);
                for (int y = 0; y < 4; y++)
                    for (int x = 0; x < 4; x++) {
                        int px = bx * 4 + x, py = by * 4 + y;
                        if (px < t->width && py < t->height)
                            out[(size_t)py * t->width + px] = block[y * 4 + x];
                    }
            }
        }
        break;
    }
    }
    return out;
}

} // namespace

extern "C" void Gfx_CopyTexture(GfxTexture *dst, GfxTexture *src)
{
    dst = Root(dst);
    src = Root(src);
    if (dst == NULL || src == NULL)
        return;
    if (src->renderedTo && s_executor != NULL) {
        s_executor->ReadTexture(src);
        src->renderedTo = false;
    }
    if (dst->format == src->format && dst->width == src->width && dst->height == src->height) {
        dst->levels[0][0] = src->levels[0][0];
        dst->dirty = true;
        return;
    }
    if (dst->format == GFX_FORMAT_BC1 || dst->format == GFX_FORMAT_BC3) {
        Sys_Log("gfx: cannot convert a texture to a compressed format");
        return;
    }
    std::vector<uint32_t> pixels = ToBGRA(src);
    uint8_t *out = dst->levels[0][0].data();
    for (int y = 0; y < dst->height; y++) {
        int sy = y * src->height / dst->height;
        for (int x = 0; x < dst->width; x++) {
            int sx = x * src->width / dst->width;
            uint32_t p = pixels[(size_t)sy * src->width + sx];
            size_t i = (size_t)y * dst->width + x;
            if (dst->format == GFX_FORMAT_BUMP_DUDVL) {
                out[i * 3] = (uint8_t)p;
                out[i * 3 + 1] = (uint8_t)(p >> 8);
                out[i * 3 + 2] = (uint8_t)(p >> 16);
            } else {
                ((uint32_t *)out)[i] = p;
            }
        }
    }
    dst->dirty = true;
}

extern "C" void Gfx_GenerateMipmaps(GfxTexture *texture)
{
    texture = Root(texture);
    if (texture == NULL || (texture->format != GFX_FORMAT_BGRX8 && texture->format != GFX_FORMAT_BGRA8))
        return;
    for (int m = 1; m < texture->mipLevels; m++) {
        int sw = std::max(1, texture->width >> (m - 1)), sh = std::max(1, texture->height >> (m - 1));
        int dw = std::max(1, texture->width >> m), dh = std::max(1, texture->height >> m);
        const uint8_t *src = texture->levels[0][m - 1].data();
        uint8_t *dst = texture->levels[0][m].data();
        for (int y = 0; y < dh; y++) {
            for (int x = 0; x < dw; x++) {
                int sum[4] = { 0, 0, 0, 0 };
                for (int dy = 0; dy < 2; dy++)
                    for (int dx = 0; dx < 2; dx++) {
                        int sx = std::min(sw - 1, x * 2 + dx), sy = std::min(sh - 1, y * 2 + dy);
                        const uint8_t *p = src + ((size_t)sy * sw + sx) * 4;
                        for (int c = 0; c < 4; c++)
                            sum[c] += p[c];
                    }
                uint8_t *q = dst + ((size_t)y * dw + x) * 4;
                for (int c = 0; c < 4; c++)
                    q[c] = (uint8_t)((sum[c] + 2) / 4);
            }
        }
    }
    texture->dirty = true;
}

extern "C" BOOL Gfx_ReadBackBuffer(int x, int y, int width, int height, DWORD *pixels)
{
    s_readback.requested = true;
    s_readback.x = x;
    s_readback.y = y;
    s_readback.width = width;
    s_readback.height = height;
    if (!s_readback.ready || s_readback.readyWidth != width || s_readback.readyHeight != height)
        return FALSE;
    memcpy(pixels, s_readback.pixels.data(), (size_t)width * height * 4);
    return TRUE;
}

// ---- BC decoding -----------------------------------------------------------------------

namespace {

uint32_t Rgb565(uint16_t c)
{
    uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
    return b | g << 8 | r << 16;
}

uint32_t Mix(uint32_t a, uint32_t b, int wa, int wb, int div)
{
    uint32_t out = 0;
    for (int s = 0; s < 24; s += 8) {
        uint32_t ca = (a >> s) & 0xff, cb = (b >> s) & 0xff;
        out |= ((ca * wa + cb * wb) / div) << s;
    }
    return out;
}

} // namespace

void Gfx_DecodeBlock(int format, const uint8_t *block, uint32_t out[16])
{
    const uint8_t *colour = format == GFX_FORMAT_BC3 ? block + 8 : block;
    uint16_t c0 = (uint16_t)(colour[0] | colour[1] << 8), c1 = (uint16_t)(colour[2] | colour[3] << 8);
    uint32_t palette[4];
    palette[0] = Rgb565(c0) | 0xff000000;
    palette[1] = Rgb565(c1) | 0xff000000;
    if (c0 > c1 || format == GFX_FORMAT_BC3) {
        palette[2] = Mix(palette[0], palette[1], 2, 1, 3) | 0xff000000;
        palette[3] = Mix(palette[0], palette[1], 1, 2, 3) | 0xff000000;
    } else {
        palette[2] = Mix(palette[0], palette[1], 1, 1, 2) | 0xff000000;
        palette[3] = 0;     // transparent black
    }
    uint32_t bits = colour[4] | colour[5] << 8 | colour[6] << 16 | (uint32_t)colour[7] << 24;
    for (int i = 0; i < 16; i++)
        out[i] = palette[(bits >> (i * 2)) & 3];

    if (format == GFX_FORMAT_BC3) {
        uint8_t a0 = block[0], a1 = block[1];
        uint8_t alphas[8];
        alphas[0] = a0;
        alphas[1] = a1;
        if (a0 > a1) {
            for (int i = 1; i < 7; i++)
                alphas[i + 1] = (uint8_t)(((7 - i) * a0 + i * a1) / 7);
        } else {
            for (int i = 1; i < 5; i++)
                alphas[i + 1] = (uint8_t)(((5 - i) * a0 + i * a1) / 5);
            alphas[6] = 0;
            alphas[7] = 255;
        }
        uint64_t abits = 0;
        for (int i = 0; i < 6; i++)
            abits |= (uint64_t)block[2 + i] << (8 * i);
        for (int i = 0; i < 16; i++)
            out[i] = (out[i] & 0x00ffffff) | (uint32_t)alphas[(abits >> (3 * i)) & 7] << 24;
    }
}

// ---- the null executor (headless runs) ---------------------------------------------------

namespace {

struct NullExecutor : GfxExecutor {
    bool SetVideoMode(int, int, bool) override { return true; }
    void SetFullscreen(bool) override {}
    void Execute(GfxFrame &, std::vector<GfxTexture *> &textures, GfxReadback &readback, bool present, bool,
                 bool) override
    {
        for (GfxTexture *t : textures)
            t->dirty = false;
        if (present && readback.requested) {
            readback.pixels.assign((size_t)readback.width * readback.height, 0xff000000);
            readback.readyWidth = readback.width;
            readback.readyHeight = readback.height;
            readback.ready = true;
            readback.requested = false;
        }
    }
    void DestroyTexture(GfxTexture *) override {}
    void ReadTexture(GfxTexture *) override {}
};

} // namespace

GfxExecutor *Gfx_CreateNullExecutor(void)
{
    return new NullExecutor;
}
