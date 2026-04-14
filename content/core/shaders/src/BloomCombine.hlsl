#include "FullscreenBindings.hlsli"
#include "FullscreenTriangle.hlsli"

cbuffer BloomParams : register(b0) {
	float4 gParams0;
	float4 gParams1; // y=intensity
};

float4 PsMain(VsOut i) : SV_Target {
	float3 bloom = gTex.Sample(gSampler, i.uv).rgb;
	return float4(bloom * max(gParams1.y, 0.0), 0.0);
}
