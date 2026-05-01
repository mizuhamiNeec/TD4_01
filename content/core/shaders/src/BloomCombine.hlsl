#include "FullscreenBindings.hlsli"
#include "FullscreenTriangle.hlsli"

cbuffer BloomParams : register(b0) {
	float4 gParams0;
	float4 gParams1; // y=intensity
};

float4 PsMain(VsOut i) : SV_Target {
	uint width  = 1u;
	uint height = 1u;
	gTex.GetDimensions(width, height);
	const float2 invAllocatedSize = 1.0 / float2(
		float(max(width, 1u)),
		float(max(height, 1u))
	);
	const float2 invSrcLogical = float2(
		gParams0.x > 0.0 ? gParams0.x : invAllocatedSize.x,
		gParams0.y > 0.0 ? gParams0.y : invAllocatedSize.y
	);
	const float2 sourceUvMax = saturate(
		invAllocatedSize / max(invSrcLogical, 1e-6.xx)
	);
	const float2 sampleUv = i.uv * sourceUvMax;
	float3 bloom = gTex.Sample(gSampler, sampleUv).rgb;
	return float4(bloom * max(gParams1.y, 0.0), 0.0);
}
