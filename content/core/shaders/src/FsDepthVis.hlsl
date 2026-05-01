//-----------------------------------------------------------------------------
// 深度バッファを可視化するシェーダー
//-----------------------------------------------------------------------------

#include "FullscreenTriangle.hlsli"
#define FULLSCREEN_TEX_TYPE float
#include "FullscreenBindings.hlsli"
#undef FULLSCREEN_TEX_TYPE

float4 PsMain(VsOut i) : SV_Target {
	uint width  = 1u;
	uint height = 1u;
	gTex.GetDimensions(width, height);
	const float2 sampleUv = ComputeSampleUvFromScreenPos(
		i.pos,
		float2(float(max(width, 1u)), float(max(height, 1u)))
	);
	float d = gTex.Sample(gSampler, sampleUv);
	float v = 1.0 - saturate(d);
	return float4(v, v, v, 1.0f);
}
