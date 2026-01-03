cbuffer MaterialCB : register(b0) {
	float4 gBaseColor;
	float  gMetallic;
	float  gForceMipNorm;
	uint   gUseForcedMip;
	float  gPadding;
	float4 gExtra[14];
}

cbuffer FrameCB : register(b1) {
	float4x4 gView;
	float4x4 gProj;
	float4x4 gViewProj;
	float3   gCameraPos;
	float    gTime;
}

cbuffer ObjectCB : register(b2) {
	float4x4 gWorld;
	float4x4 gWorldInvTranspose;
}

Texture2D    gTex : register(t0);
SamplerState gLinearWrap : register(s0);

struct InstanceData {
	float4 worldCol0;
	float4 worldCol1;
	float4 worldCol2;

	float4 normalCol0;
	float4 normalCol1;
	float4 normalCol2;
};

StructuredBuffer<InstanceData> gInstances : register(t1);

InstanceData GetInstanceData(uint instanceId) {
	return gInstances[instanceId];
}

float3 TransformPositionWS(InstanceData inst, float3 posOS) {
	float4 p = float4(posOS, 1.0f);
	return float3(dot(p, inst.worldCol0),
				  dot(p, inst.worldCol1),
				  dot(p, inst.worldCol2));
}

float3 TransformNormalWS(InstanceData inst, float3 nrmOS) {
	float3 n;
	n.x = dot(nrmOS, inst.normalCol0.xyz);
	n.y = dot(nrmOS, inst.normalCol1.xyz);
	n.z = dot(nrmOS, inst.normalCol2.xyz);
	return normalize(n);
}

struct MatIn {
	float2 uv;
	float3 normal;
};

struct MatOut {
	float3 baseColor;
	float  metallic;
};

void ShadeMaterial(in MatIn IN, out MatOut OUT);
