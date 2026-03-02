cbuffer FrameCB : register(b0) {
    float4x4 gView;
    float4x4 gProj;
    float4x4 gViewProj;
    float3   gCameraPos;
    float    gTime;
    float4   gPortalClipPlane;
    float    gPortalClipEnabled;
    float3   gFramePadding;
}

cbuffer ObjectCB : register(b1) {
    float4x4 gWorld;
    float4x4 gWorldInvTranspose;
    float4   gSkinningInfo;
};

Texture2D gPortalTexture : register(t0);
SamplerState gLinearWrapSampler : register(s0);

struct VsIn {
    float3 pos : POSITION;
    float2 uv  : TEXCOORD0;
};

struct VsOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VsOut VsMain(VsIn input) {
    VsOut output;
    const float4 worldPos = mul(float4(input.pos, 1.0f), gWorld);
    output.pos = mul(worldPos, gViewProj);
    output.uv = input.uv;
    return output;
}

float4 PsMain(VsOut input) : SV_TARGET {
    return gPortalTexture.Sample(gLinearWrapSampler, input.uv);
}
