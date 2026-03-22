cbuffer FrameCB : register(b0) {
	float4x4 gView;
	float4x4 gProj;
	float4x4 gViewProj;
	float3   gCameraPos;
	float    gTime;
	float4   gPortalClipPlane;
	float    gPortalClipEnabled;
	float3   gFramePadding;
}

struct VsIn {
	float3 position : POSITION;
	float4 color    : COLOR;
};

struct VsOut {
	float4 position : SV_POSITION;
	float4 color    : COLOR0;
};

VsOut VsMain(VsIn input) {
	VsOut output;
	output.position = mul(float4(input.position, 1.0f), gViewProj);
	output.color    = input.color;
	return output;
}

float4 PsMain(VsOut input) : SV_TARGET {
	return input.color;
}
