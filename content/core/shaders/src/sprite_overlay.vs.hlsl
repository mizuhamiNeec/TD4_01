#include "SceneConstants.hlsli"

struct VsIn {
	float3 pos : POSITION;
	float2 uv : TEXCOORD0;
};

struct VsOut {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

VsOut VsMain(VsIn input) {
	VsOut        output;
	const float4 worldPos = mul(float4(input.pos, 1.0f), gWorld);
	output.pos            = mul(worldPos, gViewProj);
	// TODO: uv専用に変える
	const float2 uvMin    = gSkinningInfo.xy;
	const float2 uvMax    = gSkinningInfo.zw;
	const float2 uvRange  = lerp(uvMin, uvMax, input.uv);
	const float  flippedV = 1.0f - uvRange.y;
	output.uv             = float2(uvRange.x, flippedV);
	return output;
}
