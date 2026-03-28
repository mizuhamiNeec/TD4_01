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
	const float flippedV  = 1.0f - input.uv.y;
	output.uv             = float2(input.uv.x, flippedV);
	return output;
}
