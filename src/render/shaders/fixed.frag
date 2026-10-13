#version 450

layout(location = 0) in vec4 inDiffuse;
layout(location = 1) in vec4 inSpecular;
layout(location = 2) in vec4 inTex0;
layout(location = 3) in vec4 inTex1;
layout(location = 4) in float inFog;
layout(location = 5) in float inEyeDepth;

layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D tex0;
layout(set = 2, binding = 1) uniform samplerCube cube0;
layout(set = 2, binding = 2) uniform sampler2D tex1;
layout(set = 2, binding = 3) uniform samplerCube cube1;

layout(std140, set = 3, binding = 0) uniform FragmentUniforms {
    ivec4 stage0;       // colour op, colour arg1, colour arg2, alpha op
    ivec4 stage0b;      // alpha arg1, alpha arg2, texture kind (0 none, 1 2D, 2 cube, 3 bump), -
    ivec4 stage1;
    ivec4 stage1b;
    vec4 textureFactor;
    ivec4 alphaTest;    // enabled, compare function, -, -
    vec4 alphaRef;      // reference / 255
    vec4 fogColor;
    ivec4 fogMode;      // fog enabled, table mode (3 linear), specular enabled, -
    vec4 fogParams;     // start, end
    vec4 bumpMatrix;    // m00, m01, m10, m11 (of the bump stage)
    vec4 bumpLuminance; // scale, offset
};

const int TOP_DISABLE = 1, TOP_SELECTARG1 = 2, TOP_SELECTARG2 = 3, TOP_MODULATE = 4, TOP_MODULATE2X = 5,
          TOP_MODULATE4X = 6, TOP_ADD = 7, TOP_ADDSIGNED = 8, TOP_ADDSIGNED2X = 9, TOP_SUBTRACT = 10,
          TOP_BUMPENVMAP = 22, TOP_BUMPENVMAPLUMINANCE = 23;

vec4 Arg(int arg, vec4 current, vec4 texel)
{
    vec4 v;
    switch (arg & 0xf) {
    case 0: v = inDiffuse; break;
    case 1: v = current; break;
    case 2: v = texel; break;
    case 3: v = textureFactor; break;
    case 4: v = inSpecular; break;
    default: v = current; break;
    }
    if ((arg & 0x10) != 0)
        v = vec4(1.0) - v;
    if ((arg & 0x20) != 0)
        v = v.aaaa;
    return v;
}

vec4 Op(int op, vec4 a, vec4 b)
{
    switch (op) {
    case TOP_SELECTARG1: return a;
    case TOP_SELECTARG2: return b;
    case TOP_MODULATE: return a * b;
    case TOP_MODULATE2X: return clamp(a * b * 2.0, 0.0, 1.0);
    case TOP_MODULATE4X: return clamp(a * b * 4.0, 0.0, 1.0);
    case TOP_ADD: return clamp(a + b, 0.0, 1.0);
    case TOP_ADDSIGNED: return clamp(a + b - 0.5, 0.0, 1.0);
    case TOP_ADDSIGNED2X: return clamp((a + b - 0.5) * 2.0, 0.0, 1.0);
    case TOP_SUBTRACT: return clamp(a - b, 0.0, 1.0);
    default: return a;
    }
}

vec4 Sample(int kind, sampler2D t2, samplerCube tc, vec4 coord)
{
    if (kind == 2)
        return texture(tc, coord.xyz);
    if (kind == 0)
        return vec4(1.0);
    return texture(t2, coord.xy);
}

float SignedByte(float v)
{
    float b = floor(v * 255.0 + 0.5);
    return (b >= 128.0 ? b - 256.0 : b) / 128.0;
}

bool Compare(int func, float a, float b)
{
    switch (func) {
    case 1: return false;
    case 2: return a < b;
    case 3: return a == b;
    case 4: return a <= b;
    case 5: return a > b;
    case 6: return a != b;
    case 7: return a >= b;
    default: return true;
    }
}

void main()
{
    vec4 current = inDiffuse;
    vec2 bumpOffset = vec2(0.0);
    float luminance = 1.0;

#ifdef FIXED_SIMPLE
    // Common road/terrain pass, selected only for this exact fixed-function state.
    vec4 texel = texture(tex0, inTex0.xy);
    float alpha = stage0.w == TOP_SELECTARG1 ? texel.a :
                  stage0.w == TOP_SELECTARG2 ? inDiffuse.a : texel.a * inDiffuse.a;
    current = vec4(texel.rgb * inDiffuse.rgb, alpha);
#else
    // Stage 0.
    if (stage0.x != TOP_DISABLE) {
        if (stage0.x == TOP_BUMPENVMAP || stage0.x == TOP_BUMPENVMAPLUMINANCE) {
            vec4 bump = texture(tex0, inTex0.xy);
            float du = SignedByte(bump.r), dv = SignedByte(bump.g);
            bumpOffset = vec2(du * bumpMatrix.x + dv * bumpMatrix.z, du * bumpMatrix.y + dv * bumpMatrix.w);
            if (stage0.x == TOP_BUMPENVMAPLUMINANCE)
                luminance = clamp(bump.b * bumpLuminance.x + bumpLuminance.y, 0.0, 1.0);
        } else {
            vec4 texel = Sample(stage0b.z, tex0, cube0, inTex0);
            vec4 color = Op(stage0.x, Arg(stage0.y, current, texel), Arg(stage0.z, current, texel));
            float alpha = stage0.w == TOP_DISABLE ? current.a
                                                  : Op(stage0.w, Arg(stage0b.x, current, texel), Arg(stage0b.y, current, texel)).a;
            current = vec4(color.rgb, alpha);
        }
        // Stage 1.
        if (stage1.x != TOP_DISABLE) {
            vec4 coord = inTex1 + vec4(bumpOffset, 0.0, 0.0);
            vec4 texel = Sample(stage1b.z, tex1, cube1, coord);
            texel.rgb *= luminance;
            vec4 color = Op(stage1.x, Arg(stage1.y, current, texel), Arg(stage1.z, current, texel));
            float alpha = stage1.w == TOP_DISABLE ? current.a
                                                  : Op(stage1.w, Arg(stage1b.x, current, texel), Arg(stage1b.y, current, texel)).a;
            current = vec4(color.rgb, alpha);
        }
    }

#endif
    if (fogMode.z != 0)
        current.rgb = clamp(current.rgb + inSpecular.rgb, 0.0, 1.0);

#ifndef FIXED_OPAQUE
    if (alphaTest.x != 0 && !Compare(alphaTest.y, floor(current.a * 255.0 + 0.5), floor(alphaRef.x * 255.0 + 0.5)))
        discard;
#endif

    if (fogMode.x != 0) {
        float f = inFog;
        if (fogMode.y == 3) {
            float range = fogParams.y - fogParams.x;
            f = range != 0.0 ? clamp((fogParams.y - inEyeDepth) / range, 0.0, 1.0) : 1.0;
        }
        current.rgb = mix(fogColor.rgb, current.rgb, f);
    }
    outColor = current;
}
