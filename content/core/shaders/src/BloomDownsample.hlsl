#include "FullscreenBindings.hlsli"
#include "FullscreenTriangle.hlsli"

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

float3 Sample9(float2 uv, float2 texel, float2 uvMax) {
    float3 c = gTex.Sample(gSampler, clamp(uv, 0.0.xx, uvMax)).rgb * 0.25;

    c += gTex.Sample(gSampler, clamp(uv + float2(texel.x, 0.0), 0.0.xx, uvMax)).rgb * 0.125;
    c += gTex.Sample(gSampler, clamp(uv + float2(-texel.x, 0.0), 0.0.xx, uvMax)).rgb * 0.125;
    c += gTex.Sample(gSampler, clamp(uv + float2(0.0, texel.y), 0.0.xx, uvMax)).rgb * 0.125;
    c += gTex.Sample(gSampler, clamp(uv + float2(0.0, -texel.y), 0.0.xx, uvMax)).rgb * 0.125;

    c += gTex.Sample(gSampler, clamp(uv + float2(texel.x, texel.y), 0.0.xx, uvMax)).rgb * 0.0625;
    c += gTex.Sample(gSampler, clamp(uv + float2(-texel.x, texel.y), 0.0.xx, uvMax)).rgb * 0.0625;
    c += gTex.Sample(gSampler, clamp(uv + float2(texel.x, -texel.y), 0.0.xx, uvMax)).rgb * 0.0625;
    c += gTex.Sample(gSampler, clamp(uv + float2(-texel.x, -texel.y), 0.0.xx, uvMax)).rgb * 0.0625;

    return c;
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

    float3 color = Sample9(sampleUv, texel, sourceUvMax);
    if (gParams1.z > 0.5) {
        color = SoftThreshold(color, gParams0.z, gParams0.w);
    }

    return float4(max(color, 0.0.xxx), 1.0);
}
