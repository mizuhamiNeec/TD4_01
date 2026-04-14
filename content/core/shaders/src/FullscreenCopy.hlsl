#include "FullscreenBindings.hlsli"
#include "FullscreenTriangle.hlsli"

float4 PsMain(VsOut i) : SV_Target {
	return gTex.Sample(gSampler, i.uv);
}
