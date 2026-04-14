#ifndef FULLSCREEN_BINDINGS_HLSLI
#define FULLSCREEN_BINDINGS_HLSLI

#ifndef FULLSCREEN_TEX_TYPE
#define FULLSCREEN_TEX_TYPE float4
#endif

Texture2D<FULLSCREEN_TEX_TYPE> gTex : register(t0);
SamplerState                   gSampler : register(s0);

#endif
