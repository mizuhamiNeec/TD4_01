#include "FullscreenBindings.hlsli"
#include "FullscreenTriangle.hlsli"

cbuffer BloomParams : register(b0) {
    float4 gParams0; // x=invSrcW, y=invSrcH
    float4 gParams1; // x=radius
};

float3 SampleTent(float2 uv, float2 texel, float2 uvMax) {
    float3 c = 0.0.xxx;
    c += gTex.Sample(gSampler, clamp(uv, 0.0.xx, uvMax)).rgb * 4.0;
    c += gTex.Sample(gSampler, clamp(uv + float2(texel.x, 0.0), 0.0.xx, uvMax)).rgb * 2.0;
    c += gTex.Sample(gSampler, clamp(uv + float2(-texel.x, 0.0), 0.0.xx, uvMax)).rgb * 2.0;
    c += gTex.Sample(gSampler, clamp(uv + float2(0.0, texel.y), 0.0.xx, uvMax)).rgb * 2.0;
    c += gTex.Sample(gSampler, clamp(uv + float2(0.0, -texel.y), 0.0.xx, uvMax)).rgb * 2.0;
    c += gTex.Sample(gSampler, clamp(uv + float2(texel.x, texel.y), 0.0.xx, uvMax)).rgb;
    c += gTex.Sample(gSampler, clamp(uv + float2(-texel.x, texel.y), 0.0.xx, uvMax)).rgb;
    c += gTex.Sample(gSampler, clamp(uv + float2(texel.x, -texel.y), 0.0.xx, uvMax)).rgb;
    c += gTex.Sample(gSampler, clamp(uv + float2(-texel.x, -texel.y), 0.0.xx, uvMax)).rgb;
    return c / 16.0;
}

float4 PsMain(VsOut i) : SV_Target {
    uint width = 1u;
    uint height = 1u;
    gTex.GetDimensions(width, height);
    float2 invAllocatedSize = 1.0 / float2(float(max(width, 1u)), float(max(height, 1u)));
    float2 invSrcLogical = float2(
        gParams0.x > 0.0 ? gParams0.x : invAllocatedSize.x,
        gParams0.y > 0.0 ? gParams0.y : invAllocatedSize.y
    );
    float2 sourceUvMax = saturate(invAllocatedSize / max(invSrcLogical, 1e-6.xx));
    float2 sampleUv = i.uv * sourceUvMax;
    float radius = max(gParams1.x, 0.5);
    float2 texel = invSrcLogical * radius;
    float3 color = SampleTent(sampleUv, texel, sourceUvMax);
    return float4(max(color, 0.0.xxx), 1.0);
}
