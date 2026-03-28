#include "FullscreenBindings.hlsli"
#include "FullscreenTriangle.hlsli"
#include "PostFxParams.hlsli"

float4 PsMain(VsOut i) : SV_Target {
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
