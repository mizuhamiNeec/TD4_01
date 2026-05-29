#include "SceneConstants.hlsli"

Texture2D    gPortalTexture : register(t0);
SamplerState gLinearWrap    : register(s0);

struct VsIn {
	float3 pos : POSITION;
	float2 uv  : TEXCOORD0;
};

struct VsOut {
	float4 pos     : SV_POSITION;
	float2 localUv : TEXCOORD0;
};

VsOut VsMain(VsIn input) {
	VsOut output;
	const float4 worldPos = mul(float4(input.pos, 1.0f), gWorld);
	output.pos = mul(worldPos, gViewProj);
	output.localUv   = input.uv;
	return output;
}

float4 PsMain(VsOut input) : SV_Target {
	float2 portalUv = float2(input.localUv.x, 1.0f - input.localUv.y);
	
	// 円形クリッピング (uv = 0~1、中心0.5)
	float2 centered = input.localUv - 0.5f;
	if (dot(centered, centered) > 0.25f) {
		discard;
	}
	
	float3 color = gPortalTexture.Sample(gLinearWrap, portalUv).rgb;
	return float4(color, 1.0f);
}
