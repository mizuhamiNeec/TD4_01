#include "SceneConstants.hlsli"

struct VsIn {
	float3 pos : POSITION;
	float2 uv : TEXCOORD0;
	float4 worldRow0 : TEXCOORD1;
	float4 worldRow1 : TEXCOORD2;
	float4 worldRow2 : TEXCOORD3;
	float4 worldRow3 : TEXCOORD4;
	float4 color : COLOR0;
	float4 uvRect : TEXCOORD5;
};

struct VsOut {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float4 color : COLOR0;
};

VsOut VsMain(VsIn input) {
	VsOut output;
	const float4x4 world = float4x4(
		input.worldRow0,
		input.worldRow1,
		input.worldRow2,
		input.worldRow3
	);
	const float4 worldPos = mul(float4(input.pos, 1.0f), world);
	output.pos = mul(worldPos, gViewProj);
	const float2 uvMin = input.uvRect.xy;
	const float2 uvMax = input.uvRect.zw;
	const float2 uvRange = lerp(uvMin, uvMax, input.uv);
	output.uv = uvRange;
	output.color = input.color;
	return output;
}
