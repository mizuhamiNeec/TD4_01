#ifndef FULLSCREEN_TRIANGLE_HLSLI
#define FULLSCREEN_TRIANGLE_HLSLI

// 画面全体を覆う1枚の三角形

static const int kNumVertices = 3;

static const float4 kPositions[kNumVertices] = {
	{-1.0f, 1.0f, 0.0f, 1.0f},  // 左上
	{3.0f, 1.0f, 0.0f, 1.0f},   // 右上
	{-1.0f, -3.0f, 0.0f, 1.0f}, // 左下
};

static const float2 kTexCoords[kNumVertices] = {
	{0.0f, 0.0f}, // 左上
	{2.0f, 0.0f}, // 右上
	{0.0f, 2.0f}, // 左下
};

struct VsOut {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

VsOut VsMain(uint vid : SV_VertexID) {
	VsOut o;
	o.pos = kPositions[vid];
	o.uv  = kTexCoords[vid];
	return o;
}

#endif
