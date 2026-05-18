#pragma once
#include <string>
#include <vector>

#include "../BlendMode.h"

#include "core/math/Vec3.h"
#include "core/math/Vec4.h"

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
	Vec3 minValue{ 0,0,0 };
	Vec3 maxValue{ 0,0,0 };
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
	Vec4 color{ 1,1,1,1 };  // この t での色
};

struct ColorGradient {
	bool enabled = false;
	std::vector<ColorKey4> keys;

	Vec4 Evaluate(float t) const {
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
				Vec4 c{};
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
	Vec3 initialScale{ 1,1,1 };
	RandomRange3 initialScaleRandom;

	Vec3 initialRotate{ 0,0,0 };
	RandomRange3 initialRotateRandom;

	Vec3 initialOffset{ 0,0,0 };
	RandomRange3 initialOffsetRandom; // エミッタからの相対オフセット
};

// --- Particle Update モジュール ---
struct ParticleUpdateModule {
	float   lifeTime = 1.0f;               // 寿命
	Vec3 velocity = { 0, 0, 0 };        // 速度
	RandomRange3 velocityRandom;
	Vec3 rotationSpeed = { 0, 0, 0 };   // 回転速度
	RandomRange3 rotationRandom;
	Vec3 scaleSpeed = { 0, 0, 0 };      // スケール速度
	RandomRange3 scaleRandom;
	bool    useGravity = false;              // 重力

	// スケール倍率カーブ（NormalizedAge に対する multiplier）
	Curve1D scaleCurve;
};

// --- Render モジュール ---
struct RenderModule {
	Vec4 color = { 1, 1, 1, 1 };  // 色
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
// ModuleSlot
// 「どのモジュールを・どの順番で・有効/無効で」エミッタに積むかを表す。
// UE5 Niagara の「モジュールスタック」の 1 行に相当する。
//
// パラメータの実体は上の各 *Module 構造体（EmitterSpawnModule など）が保持し、
// JSON では modules 配列の各要素の "params" として読み書きされる。
// ※ ModuleSlot 自体は文字列＋bool だけなのでコピー可能。
// ===============================================
struct ModuleSlot {
	std::string type;           // モジュール種別名（"Lifetime" / "Velocity" など）
	bool        enabled = true; // 有効/無効（Niagara のチェックボックス相当）
};

// ===============================================
// ParticlePreset
// ===============================================
struct ParticlePreset {
	std::string name;               // プリセット名

	// ---- 各モジュールのパラメータ実体 ----
	EmitterSettingsModule emitterSettings;
	EmitterSpawnModule    emitterSpawn;
	ParticleSpawnModule   particleSpawn;
	ParticleUpdateModule  particleUpdate;
	RenderModule          render;

	// ---- モジュールスタック ----
	// 振る舞いモジュールの種類・順序・有効状態。
	// 空の場合、ParticleEmitterInstance / JSON 出力は
	// ParticleModuleRegistry の既定スタックを使う。
	std::vector<ModuleSlot> modules;
};