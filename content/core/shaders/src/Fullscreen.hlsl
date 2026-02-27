Texture2D    gTex : register(t0);
SamplerState gSampler : register(s0);

struct VSOut {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

static const uint kNumVertices = 3;

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

VSOut VsMain(uint vid : SV_VertexID) {
	VSOut o;
	o.pos = kPositions[vid];
	o.uv  = kTexCoords[vid];
	return o;
}

float4 PsMain(VSOut i) : SV_Target { return gTex.Sample(gSampler, i.uv); }
