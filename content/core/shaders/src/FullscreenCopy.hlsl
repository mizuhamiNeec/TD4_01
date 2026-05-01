#include "FullscreenBindings.hlsli"
#include "FullscreenTriangle.hlsli"
#include "PostFxParams.hlsli"

float4 PsMain(VsOut i) : SV_Target {
	const float2 uvScale = float2(
		gPostFxScalar0.x > 0.0f ? gPostFxScalar0.x : 1.0f,
		gPostFxScalar0.y > 0.0f ? gPostFxScalar0.y : 1.0f
	);
	const float2 sampleUv = i.uv * saturate(uvScale);
	return gTex.Sample(gSampler, sampleUv);
}
