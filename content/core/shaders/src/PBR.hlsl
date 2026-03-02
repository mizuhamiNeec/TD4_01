cbuffer FrameCB : register(b0) {
	float4x4 gView;      // ビュー行列
	float4x4 gProj;      // プロジェクション行列
	float4x4 gViewProj;  // ビュープロジェクション行列
	float3   gCameraPos; // カメラのワールド空間位置
	float    gTime;      // 経過時間
	float4   gPortalClipPlane;
	float    gPortalClipEnabled;
	float3   gFramePadding;
}

cbuffer ObjectCB : register(b1) {
	float4x4 gWorld;
	float4x4 gWorldInvTranspose;
	float4   gSkinningInfo; // x=paletteOffset, y=useSkinning
};

cbuffer MaterialCB : register(b2) {
	float4 gBaseColor;
	float4 gEmissiveColor;
	float  gMetallic;
	float  gRoughness;
	float  gOpacity;
	float  gDomainMode; // 0=Unlit, 1=PBR
	float2 gPadding;
};

cbuffer SkinningPaletteCB : register(b3) {
	float4x4 gSkinMatrices[64];
};

Texture2D    gBaseColorTex : register(t0);
SamplerState gLinearWrap : register(s0);

struct VsIn {
	float3 pos : POSITION;
	float3 nrm : NORMAL;
	float2 uv : TEXCOORD0;
	float4 boneIndices : TEXCOORD1;
	float4 boneWeights : TEXCOORD2;
};

struct VsOut {
	float4 pos : SV_POSITION;
	float3 normalWS : TEXCOORD0;
	float2 uv : TEXCOORD1;
	float3 positionWS : TEXCOORD2;
};

VsOut VsMain(VsIn i) {
	VsOut  o;
	float3 localPos = i.pos;
	float3 localNrm = i.nrm;

	if (gSkinningInfo.y > 0.5f) {
		float4 skinnedPos = 0.0f;
		float3 skinnedNrm = 0.0f;

		[unroll]
		for (uint k = 0; k < 4; ++k) {
			const float weight = i.boneWeights[k];
			if (weight <= 0.0f) { continue; }

			const uint boneIndex = (uint)i.boneIndices[k];
			skinnedPos           += mul(
				float4(i.pos, 1.0f), gSkinMatrices[boneIndex]
			) * weight;
			skinnedNrm += mul(
				float4(i.nrm, 0.0f), gSkinMatrices[boneIndex]
			).xyz * weight;
		}

		localPos = skinnedPos.xyz;
		localNrm = normalize(skinnedNrm);
	}

	float4 wp = mul(float4(localPos, 1.0f), gWorld);
	o.pos = mul(wp, gViewProj);
	o.positionWS = wp.xyz;
	o.normalWS = normalize(mul(float4(localNrm, 0.0f), gWorldInvTranspose).xyz);
	o.uv = i.uv;
	return o;
}

float4 PsMain(VsOut i) : SV_TARGET {
	if (gPortalClipEnabled > 0.5f) {
		clip(dot(float4(i.positionWS, 1.0f), gPortalClipPlane));
	}

	float3 texColor = gBaseColorTex.Sample(gLinearWrap, i.uv).rgb;
	float3 albedo   = texColor * gBaseColor.rgb;

	if (gDomainMode < 0.5f) {
		float3 unlit = albedo + gEmissiveColor.rgb;
		return float4(unlit, saturate(gOpacity * gBaseColor.a));
	}

	float3 N = normalize(i.normalWS);
	float3 V = normalize(gCameraPos - i.positionWS);
	float3 L = normalize(float3(0.3f, 0.75f, 0.4f));
	float3 H = normalize(V + L);

	float  ndl       = saturate(dot(N, L));
	float  ndh       = saturate(dot(N, H));
	float  rough     = saturate(gRoughness);
	float  specPow   = lerp(128.0f, 8.0f, rough);
	float  spec      = pow(ndh, specPow);
	float3 specColor = lerp(
		float3(0.04f, 0.04f, 0.04f), albedo, saturate(gMetallic)
	);

	float3 lit = albedo * (0.08f + ndl) + specColor * spec + gEmissiveColor.rgb;
	return float4(lit, saturate(gOpacity * gBaseColor.a));
}
