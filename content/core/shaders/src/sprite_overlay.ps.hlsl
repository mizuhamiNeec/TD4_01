#include "SceneConstants.hlsli"

Texture2D    gSpriteTexture : register(t0);
SamplerState gLinearWrapSampler : register(s0);

struct VsOut {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

float4 PsMain(VsOut input) : SV_Target {
	float2 sampleUv = input.uv;
	if (gSkinningInfo.x < -0.5f) {
		const float2 safeViewportSize = max(gViewportSize, float2(1.0f, 1.0f));
		sampleUv = input.pos.xy / safeViewportSize;
		if (gSkinningInfo.y <= 0.5f) {
			sampleUv.y = 1.0f - sampleUv.y;
		}
	}

	const float4 texel = gSpriteTexture.Sample(gLinearWrapSampler, sampleUv);
	return float4(
		texel.rgb * gBaseColor.rgb,
		texel.a * saturate(gOpacity * gBaseColor.a)
	);
}
