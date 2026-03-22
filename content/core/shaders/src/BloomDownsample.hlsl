#include "FullscreenTriangle.hlsli"
Texture2D    gTex : register(t0);
SamplerState gSampler : register(s0);

cbuffer BloomParams : register(b0) {
    float4 gParams0; // x=invSrcW, y=invSrcH, z=threshold, w=knee
    float4 gParams1; // x=radius, y=intensity, z=firstPass
};

float3 SoftThreshold(float3 color, float threshold, float knee) {
    float brightness = max(color.r, max(color.g, color.b));
    if (brightness <= 1e-5) {
        return 0.0.xxx;
    }

    float k = max(knee, 1e-5);
    float soft = brightness - threshold + k;
    soft = saturate(soft / (2.0 * k));
    soft = soft * soft * (3.0 - 2.0 * soft);

    float hard = max(brightness - threshold, 0.0);
    float contribution = hard + soft * k;
    return color * (contribution / brightness);
}

float3 Sample9(float2 uv, float2 texel) {
    float3 c = gTex.Sample(gSampler, uv).rgb * 0.25;

    c += gTex.Sample(gSampler, uv + float2(texel.x, 0.0)).rgb * 0.125;
    c += gTex.Sample(gSampler, uv + float2(-texel.x, 0.0)).rgb * 0.125;
    c += gTex.Sample(gSampler, uv + float2(0.0, texel.y)).rgb * 0.125;
    c += gTex.Sample(gSampler, uv + float2(0.0, -texel.y)).rgb * 0.125;

    c += gTex.Sample(gSampler, uv + float2(texel.x, texel.y)).rgb * 0.0625;
    c += gTex.Sample(gSampler, uv + float2(-texel.x, texel.y)).rgb * 0.0625;
    c += gTex.Sample(gSampler, uv + float2(texel.x, -texel.y)).rgb * 0.0625;
    c += gTex.Sample(gSampler, uv + float2(-texel.x, -texel.y)).rgb * 0.0625;

    return c;
}

float4 PsMain(VSOut i) : SV_Target {
    float2 invSrcSize = gParams0.xy;
    float radius = max(gParams1.x, 0.5);
    float2 texel = invSrcSize * radius;

    float3 color = Sample9(i.uv, texel);
    if (gParams1.z > 0.5) {
        color = SoftThreshold(color, gParams0.z, gParams0.w);
    }

    return float4(max(color, 0.0.xxx), 1.0);
}
