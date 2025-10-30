// こいつはサジェスト用。 編集が完了したらコメントアウトか、削除しよう
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
	OUT.baseColor = tex;
	OUT.metallic  = gMetallic;
}
