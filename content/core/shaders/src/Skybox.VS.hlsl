#include "SceneConstants.hlsli"
#include "Skybox.hlsli"

SkyboxVsOut VsMain(SkyboxVsIn input) {
	SkyboxVsOut output;

	float4 worldPos = mul(float4(input.pos, 1.0f), gWorld);
	float4 viewPos  = mul(worldPos, gView);
	float4 clipPos  = mul(viewPos, gProj);

	// VertexShaderの出力は同時クリップ空間であり、
	// 同時クリップ空間で最も遠い場所はwである。
	// その後のPerspectiveDivideによってNDCになることを考えても、
	// zとwを同じ値にしておけば、z=1となり、NDCでの最遠方に配置されることになる。
	output.position  = float4(clipPos.xy, 0.0f, clipPos.w);
	output.direction = input.pos;
	return output;
}
