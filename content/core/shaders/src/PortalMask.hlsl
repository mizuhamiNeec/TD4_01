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

struct VsIn {
    float3 pos : POSITION;
    float2 uv  : TEXCOORD0;
};

struct VsOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VsOut VsMain(VsIn i) {
    VsOut o;
    float4 wp = mul(float4(i.pos, 1.0f), gWorld);
    o.pos = mul(wp, gViewProj);
    o.uv = i.uv;
    return o;
}

void PsMain(VsOut i) {
    (void)i;
}
