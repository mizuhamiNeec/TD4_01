#include "SceneConstants.hlsli"
#include "Skybox.hlsli"

TextureCube gSkyboxTex : register(t0);
SamplerState gSkyboxSampler : register(s0);

float4 PsMain(SkyboxVsOut input) : SV_Target {
	float3 direction = normalize(input.direction);
	float4 color     = gSkyboxTex.Sample(gSkyboxSampler, direction);
	return float4(color.rgb * gBaseColor.rgb, 1.0f);
}
