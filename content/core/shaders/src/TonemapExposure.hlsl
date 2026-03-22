#include "FullscreenTriangle.hlsli"

Texture2D    gTex : register(t0);
SamplerState gSampler : register(s0);

cbuffer PostFxParams : register(b0) {
	float4 gPostFxScalar0; // x=Intensity
	float4 gPostFxScalar1; // x=ExposureEV
	float4 gPostFxColor0;
	float4 gPostFxColor1;
};

/// @brief Reinhardトーンマッピング
/// @param color HDRカラー
/// @return LDRカラー
float3 ToneMapReinhard(float3 color) {
	return color / (1.0 + color);
}

float4 PsMain(VSOut i) : SV_Target {
	float4 src         = gTex.Sample(gSampler, i.uv);
	float  exposureMul = exp2(gPostFxScalar1.x);
	float3 hdr         = max(src.rgb * exposureMul, 0.0.xxx);
	float3 ldr         = ToneMapReinhard(hdr);
	return float4(saturate(ldr), src.a);
}
