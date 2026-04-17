#include "FullscreenBindings.hlsli"
#include "FullscreenTriangle.hlsli"
#include "PostFxParams.hlsli"

/// @brief RGB から知覚重み付き輝度を計算する。
/// @param color 入力カラー
/// @return 輝度値
float ComputeLuma(const float3 color) {
	return dot(color, float3(0.299f, 0.587f, 0.114f));
}

/// @brief スカラー未設定時に安全な既定値へフォールバックする。
/// @param value 入力値
/// @param fallback 既定値
/// @return 正規化済みの値
float ResolvePositiveOrDefault(const float value, const float fallback) {
	return value > 0.0f ? value : fallback;
}

/// @brief FXAAでエッジのジャギーを軽減した色を返す。
/// @param i フルスクリーン三角形の補間入力
/// @return アンチエイリアス後の色
float4 PsMain(VsOut i) : SV_Target {
	uint width  = 1u;
	uint height = 1u;
	gTex.GetDimensions(width, height);
	const float2 invResolution = 1.0f / float2(
		float(max(width, 1u)),
		float(max(height, 1u))
	);

	const float edgeThreshold = ResolvePositiveOrDefault(gPostFxScalar0.x, 0.166f);
	const float edgeThresholdMin = ResolvePositiveOrDefault(
		gPostFxScalar0.y, 0.0625f
	);
	const float subpixelBlending = saturate(
		ResolvePositiveOrDefault(gPostFxScalar0.z, 0.75f)
	);
	const float maxSpan = ResolvePositiveOrDefault(gPostFxScalar0.w, 8.0f);

	const float2 uv = i.uv;
	const float3 rgbM = gTex.Sample(gSampler, uv).rgb;
	const float3 rgbNW = gTex.Sample(gSampler, uv + float2(-1.0f, -1.0f) * invResolution).rgb;
	const float3 rgbNE = gTex.Sample(gSampler, uv + float2(1.0f, -1.0f) * invResolution).rgb;
	const float3 rgbSW = gTex.Sample(gSampler, uv + float2(-1.0f, 1.0f) * invResolution).rgb;
	const float3 rgbSE = gTex.Sample(gSampler, uv + float2(1.0f, 1.0f) * invResolution).rgb;

	const float lumaM = ComputeLuma(rgbM);
	const float lumaNW = ComputeLuma(rgbNW);
	const float lumaNE = ComputeLuma(rgbNE);
	const float lumaSW = ComputeLuma(rgbSW);
	const float lumaSE = ComputeLuma(rgbSE);

	const float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	const float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
	const float lumaRange = lumaMax - lumaMin;

	const float edgeCutoff = max(edgeThresholdMin, lumaMax * edgeThreshold);
	if (lumaRange < edgeCutoff) {
		return float4(rgbM, 1.0f);
	}

	float2 dir = float2(
		-((lumaNW + lumaNE) - (lumaSW + lumaSE)),
		((lumaNW + lumaSW) - (lumaNE + lumaSE))
	);

	const float dirReduce = max(
		((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25f) * subpixelBlending,
		1.0f / 128.0f
	);
	const float rcpDirMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);
	dir = clamp(dir * rcpDirMin, float2(-maxSpan, -maxSpan), float2(maxSpan, maxSpan)) * invResolution;

	const float3 rgbA = 0.5f * (
		gTex.Sample(gSampler, uv + dir * (1.0f / 3.0f - 0.5f)).rgb +
		gTex.Sample(gSampler, uv + dir * (2.0f / 3.0f - 0.5f)).rgb
	);
	const float3 rgbB = rgbA * 0.5f + 0.25f * (
		gTex.Sample(gSampler, uv + dir * -0.5f).rgb +
		gTex.Sample(gSampler, uv + dir * 0.5f).rgb
	);

	const float lumaB = ComputeLuma(rgbB);
	const float3 finalColor = (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;
	return float4(finalColor, 1.0f);
}
