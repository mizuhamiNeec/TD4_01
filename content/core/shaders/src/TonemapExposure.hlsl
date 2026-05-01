#include "FullscreenBindings.hlsli"
#include "FullscreenTriangle.hlsli"
#include "PostFxParams.hlsli"

/// @brief Reinhardトーンマッピング
/// @param color HDRカラー
/// @return LDRカラー
float3 ToneMapReinhard(float3 color) {
	return color / (1.0 + color);
}

float4 PsMain(VsOut i) : SV_Target {
	uint width  = 1u;
	uint height = 1u;
	gTex.GetDimensions(width, height);
	const float2 sampleUv = ComputeSampleUvFromScreenPos(
		i.pos,
		float2(float(max(width, 1u)), float(max(height, 1u)))
	);
	float4 src         = gTex.Sample(gSampler, sampleUv);
	float  exposureMul = exp2(gPostFxScalar1.x);
	float3 hdr         = max(src.rgb * exposureMul, 0.0.xxx);
	float3 ldr         = ToneMapReinhard(hdr);
	return float4(saturate(ldr), src.a);
}
