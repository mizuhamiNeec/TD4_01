#include "SceneConstants.hlsli"

Texture2D    gPortalTexture : register(t0);
SamplerState gLinearWrap : register(s0);

struct VsIn {
	float3 pos : POSITION;
	float2 uv : TEXCOORD0;
};

struct VsOut {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

VsOut VsMain(VsIn input) {
	VsOut output;
	
	const float4 worldPos = mul(float4(input.pos, 1.0f), gWorld);
	output.pos = mul(worldPos,gViewProj);
	
	output.uv = input.uv;
	
	return output;
}

float4 PsMain(VsOut input) : SV_Target {
	float2 screenSpaceUv = input.pos.xy / gViewportSize;
	
	float4 texel = gPortalTexture.Sample(gLinearWrap,screenSpaceUv);
	
	// 円形クリッピング
	float2 centered = input.uv - 0.5f;
	if (dot(centered, centered) > 0.25f) {
		discard;
	}
	
	return float4(
		texel.rgb * gBaseColor.rgb,
		texel.a * saturate(gOpacity * gBaseColor.a)
	);
}
