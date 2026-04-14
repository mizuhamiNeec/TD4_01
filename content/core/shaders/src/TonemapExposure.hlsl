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
	float4 src         = gTex.Sample(gSampler, i.uv);
	float  exposureMul = exp2(gPostFxScalar1.x);
	float3 hdr         = max(src.rgb * exposureMul, 0.0.xxx);
	float3 ldr         = ToneMapReinhard(hdr);
	return float4(saturate(ldr), src.a);
}
