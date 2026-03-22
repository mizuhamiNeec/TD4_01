#include "FullscreenTriangle.hlsli"

Texture2D    gTex : register(t0);
SamplerState gSampler : register(s0);

cbuffer BloomParams : register(b0) {
	float4 gParams0;
	float4 gParams1; // y=intensity
};

float4 PsMain(VSOut i) : SV_Target {
	float3 bloom = gTex.Sample(gSampler, i.uv).rgb;
	return float4(bloom * max(gParams1.y, 0.0), 0.0);
}
