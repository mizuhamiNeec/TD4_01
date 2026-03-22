#include "FullscreenTriangle.hlsli"

Texture2D    gTex : register(t0);
SamplerState gSampler : register(s0);

float4 PsMain(VSOut i) : SV_Target {
	return gTex.Sample(gSampler, i.uv);
}
