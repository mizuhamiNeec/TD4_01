#include "FullscreenBindings.hlsli"
#include "FullscreenTriangle.hlsli"

cbuffer BloomParams : register(b0) {
    float4 gParams0; // x=invSrcW, y=invSrcH
    float4 gParams1; // x=radius
};

float3 SampleTent(float2 uv, float2 texel) {
    float3 c = 0.0.xxx;
    c += gTex.Sample(gSampler, uv).rgb * 4.0;
    c += gTex.Sample(gSampler, uv + float2(texel.x, 0.0)).rgb * 2.0;
    c += gTex.Sample(gSampler, uv + float2(-texel.x, 0.0)).rgb * 2.0;
    c += gTex.Sample(gSampler, uv + float2(0.0, texel.y)).rgb * 2.0;
    c += gTex.Sample(gSampler, uv + float2(0.0, -texel.y)).rgb * 2.0;
    c += gTex.Sample(gSampler, uv + float2(texel.x, texel.y)).rgb;
    c += gTex.Sample(gSampler, uv + float2(-texel.x, texel.y)).rgb;
    c += gTex.Sample(gSampler, uv + float2(texel.x, -texel.y)).rgb;
    c += gTex.Sample(gSampler, uv + float2(-texel.x, -texel.y)).rgb;
    return c / 16.0;
}

float4 PsMain(VsOut i) : SV_Target {
    float2 invSrcSize = gParams0.xy;
    float radius = max(gParams1.x, 0.5);
    float2 texel = invSrcSize * radius;
    float3 color = SampleTent(i.uv, texel);
    return float4(max(color, 0.0.xxx), 1.0);
}
