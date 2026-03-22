cbuffer MaterialCB : register(b2) {
	float4 gBaseColor;
	float4 gEmissiveColor;
	float  gMetallic;
	float  gRoughness;
	float  gOpacity;
	float  gDomainMode;
	float2 gPadding;
}

Texture2D    gSpriteTexture : register(t0);
SamplerState gLinearWrapSampler : register(s0);

struct PsIn {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

float4 PsMain(PsIn input) : SV_TARGET {
	const float4 texel = gSpriteTexture.Sample(gLinearWrapSampler, input.uv);
	return float4(
		texel.rgb * gBaseColor.rgb,
		texel.a * saturate(gOpacity * gBaseColor.a)
	);
}
