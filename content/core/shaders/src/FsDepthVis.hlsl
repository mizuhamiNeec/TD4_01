//-----------------------------------------------------------------------------
// 深度バッファを可視化するシェーダー
//-----------------------------------------------------------------------------

#include "FullscreenTriangle.hlsli"
#define FULLSCREEN_TEX_TYPE float
#include "FullscreenBindings.hlsli"
#undef FULLSCREEN_TEX_TYPE

float4 PsMain(VsOut i) : SV_Target {
	float d = gTex.Sample(gSampler, i.uv);
	float v = 1.0 - saturate(d);
	return float4(v, v, v, 1.0f);
}
