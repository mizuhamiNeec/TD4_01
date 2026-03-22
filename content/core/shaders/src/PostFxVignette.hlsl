Texture2D    gTex : register(t0);
SamplerState gSampler : register(s0);

cbuffer PostFxParams : register(b0) {
	float4 gPostFxScalar0; // x=Intensity, y=Threshold, z=Radius, w=Amount
	float4 gPostFxScalar1; // 未使用
	float4 gPostFxColor0;  // default tint
	float4 gPostFxColor1;  // 未使用
};

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

struct VSOut {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

VSOut VsMain(uint vid : SV_VertexID) {
	VSOut o;
	o.pos = kPositions[vid];
	o.uv  = kTexCoords[vid];
	return o;
}

float4 PsMain(VSOut i) : SV_TARGET {
	float2 uv  = i.uv;
	float4 col = gTex.Sample(gSampler, uv);

	float2 d         = uv - float2(0.5, 0.5); // 画面中心からの距離
	float  intensity = gPostFxScalar0.x;      // 強度

	// ビネットの計算
	float vignette = saturate(1.0 - dot(d, d) * intensity);

	// ビネットと色の適用
	col.rgb *= vignette * gPostFxColor0.rgb;
	return col;
}
