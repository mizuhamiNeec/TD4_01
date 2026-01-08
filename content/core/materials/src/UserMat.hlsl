// こいつはサジェスト用。 編集が完了したらコメントアウトしよう
//#include "MaterialABI.hlsli"

void ShadeMaterial(in MatIn IN, out MatOut OUT) {
	float2 uv = float2(IN.uv.x, -IN.uv.y);

	uint tw, th, mipCount;
	gTex.GetDimensions(0, tw, th, mipCount);

	float4 texColor = gTex.Sample(gLinearWrap, uv);

	float3 tex = texColor.rgb;
	// float  rough = gExtra[0].x;
	// float3 emi   = gExtra[1].rgb;
	float3 lit    = lerp(tex * 0.5, tex, saturate(1 - gMetallic));
	
	lit = lerp(lit, float3(0.25,0.25,0.75f), IN.normal.y);
	
	OUT.baseColor = lit;
	OUT.metallic  = gMetallic;
}
