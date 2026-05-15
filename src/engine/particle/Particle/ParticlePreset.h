#pragma once
#include <string>
#include <vector>

#include "MathFunctions.h"
#include "engine/BlendMode.h"

// ===============================================
// 共通で使う列挙型
// ===============================================

// メッシュの選択
enum class VertexDataType {
	Plane,
	Ring,
	Cylinder,
	// 今後追加予定の形状もここに列挙
};

// ===============================================
// カーブ
// ===============================================
struct CurveKey {
	float t = 0.0f;   // 0.0～1.0 (NormalizedAge)
	float v = 1.0f;   // 倍率など
};

// ===============================================
// ランダム
// ===============================================
struct RandomRange3 {
	bool useRandom = false;
	Vector3 minValue{ 0,0,0 };
	Vector3 maxValue{ 0,0,0 };
};


struct Curve1D {
	bool enabled = false;
	std::vector<CurveKey> keys;

	float Evaluate(float t) const {
		if (!enabled || keys.empty()) {
			return 1.0f;
		}
		if (t <= keys.front().t) { return keys.front().v; }
		if (t >= keys.back().t) { return keys.back().v; }

		for (size_t i = 0; i + 1 < keys.size(); ++i) {
			const CurveKey& k0 = keys[i];
			const CurveKey& k1 = keys[i + 1];
			if (t >= k0.t && t <= k1.t && k1.t > k0.t) {
				float u = (t - k0.t) / (k1.t - k0.t);
				return k0.v + (k1.v - k0.v) * u;
			}
		}
		return 1.0f;
	}
};

//======================================
// 色用グラデーション
//======================================
struct ColorKey4 {
	float   t = 0.0f;          // 0〜1 (NormalizedAge か時間カーブの値)
	Vector4 color{ 1,1,1,1 };  // この t での色
};

struct ColorGradient {
	bool enabled = false;
	std::vector<ColorKey4> keys;

	Vector4 Evaluate(float t) const {
		if (!enabled || keys.empty()) {
			return { 1,1,1,1 };
		}
		if (t <= keys.front().t) { return keys.front().color; }
		if (t >= keys.back().t) { return keys.back().color; }

		for (size_t i = 0; i + 1 < keys.size(); ++i) {
			const ColorKey4& k0 = keys[i];
			const ColorKey4& k1 = keys[i + 1];
			if (t >= k0.t && t <= k1.t && k1.t > k0.t) {
				float u = (t - k0.t) / (k1.t - k0.t);
				Vector4 c{};
				c.x = k0.color.x + (k1.color.x - k0.color.x) * u;
				c.y = k0.color.y + (k1.color.y - k0.color.y) * u;
				c.z = k0.color.z + (k1.color.z - k0.color.z) * u;
				c.w = k0.color.w + (k1.color.w - k0.color.w) * u;
				return c;
			}
		}
		return keys.back().color;
	}
};


// ===============================================
// 各モジュール
// ===============================================

// --- Emitter Settings モジュール ---
struct EmitterSettingsModule {
	VertexDataType vertexType = VertexDataType::Plane; // Plane / Ring / Cylinder
	std::string    textureFilePath;                    // テクスチャパス
	BlendMode      blendMode = kBlendModeNormal;       // ブレンドモード
};

// --- Emitter Spawn モジュール ---
struct EmitterSpawnModule {
	uint32_t count = 10;            // 発生数
	float    frequency = 1.0f;      // 発生間隔(秒)
	bool     repeat = false;        // 繰り返し
	bool     useRandomPosition = false; // ランダム発生
	bool     useLocalSpace = false;     // ローカル空間モード（trueならエミッタ基準の相対座標）
};

// --- Particle Spawn モジュール ---
struct ParticleSpawnModule {
	Vector3 initialScale{ 1,1,1 };
	RandomRange3 initialScaleRandom;

	Vector3 initialRotate{ 0,0,0 };
	RandomRange3 initialRotateRandom;

	Vector3 initialOffset{ 0,0,0 };
	RandomRange3 initialOffsetRandom; // エミッタからの相対オフセット
};

// --- Particle Update モジュール ---
struct ParticleUpdateModule {
	float   lifeTime = 1.0f;               // 寿命
	Vector3 velocity = { 0, 0, 0 };        // 速度
	RandomRange3 velocityRandom;
	Vector3 rotationSpeed = { 0, 0, 0 };   // 回転速度
	RandomRange3 rotationRandom;
	Vector3 scaleSpeed = { 0, 0, 0 };      // スケール速度
	RandomRange3 scaleRandom;
	bool    useGravity = false;              // 重力

	// スケール倍率カーブ（NormalizedAge に対する multiplier）
	Curve1D scaleCurve;
};

// --- Render モジュール ---
struct RenderModule {
	Vector4 color = { 1, 1, 1, 1 };  // 色
	bool    useBillboard = true;           // ビルボード
	bool    flipY = false;          // Cylinder の上下反転など

	// 色の強さ/α用カーブ
	Curve1D colorCurve;

	// 色そのものを変えるためのグラデーション
	ColorGradient colorGradient;

	// 「何秒目でグラデーションのどの位置まで進むか」を決める時間カーブ
	// age(0〜1) -> グラデーションの t(0〜1)
	Curve1D gradientTimeCurve;
};

// ===============================================
// ParticlePreset
// ===============================================
struct ParticlePreset {
	std::string name;               // プリセット名

	// ---- 各モジュール ----
	EmitterSettingsModule emitterSettings;
	EmitterSpawnModule    emitterSpawn;
	ParticleSpawnModule   particleSpawn;
	ParticleUpdateModule  particleUpdate;
	RenderModule          render;
};