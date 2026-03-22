//-----------------------------------------------------------------------------
// 深度バッファを可視化するシェーダー
//-----------------------------------------------------------------------------

Texture2D<float> gDepth : register(t0);
SamplerState     gSampler : register(s0);

struct VsOut {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

float4 PsMain(VsOut i) : SV_Target {
	float d = gDepth.Sample(gSampler, i.uv);
	float v = 1.0 - saturate(d);
	return float4(v, v, v, 1.0f);
}
