#include "SceneConstants.hlsli"

Texture2D    gSpriteTexture : register(t0);
SamplerState gLinearWrapSampler : register(s0);

struct VsOut {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

float4 PsMain(VsOut input) : SV_Target {
	const float4 texel = gSpriteTexture.Sample(gLinearWrapSampler, input.uv);
	return float4(
		texel.rgb * gBaseColor.rgb,
		texel.a * saturate(gOpacity * gBaseColor.a)
	);
}
