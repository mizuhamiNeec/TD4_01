#pragma once

class ParticleEmitterInstance;
struct Particle;

// ===============================================
// ParticleModule（抽象基底クラス）
//
// UE5 Niagara の「モジュール」に相当する。
// 「色変化」「速度」「スケール」など 1つの振る舞いを 1モジュールとして実装し、
// ParticleEmitterInstance がモジュール列を順番に適用する。
//
//   ApplySpawn  : パーティクル発生時に1回だけ呼ばれる初期化処理
//   ApplyUpdate : 毎フレーム呼ばれる更新処理
//   ApplyRender : 描画直前に呼ばれる処理（必要なら）
//
// 各モジュールは設定値（発生数・速度・カーブなど）を
// ParticlePreset から「データ」として読み取る（emitter.GetPreset() 経由）。
//
// GetTypeName() はモジュール種別の安定IDで、
//   - ParticleModuleRegistry でのファクトリ生成
//   - JSON の "type" フィールド
//   - エディタ表示
// に共通で使われる。
// ===============================================
class ParticleModule {
public:
	virtual ~ParticleModule() = default;

	// モジュール種別名（ファクトリ生成・JSON保存・エディタ表示の安定ID）
	virtual const char* GetTypeName() const = 0;

	// 有効/無効（Niagara のモジュールのチェックボックス相当）
	// 無効なモジュールは ParticleEmitterInstance から呼ばれない。
	bool IsEnabled() const { return enabled_; }
	void SetEnabled(bool enabled) { enabled_ = enabled; }

	// Emit 時の初期化処理
	virtual void ApplySpawn(ParticleEmitterInstance& emitter, Particle& p) {
		(void)emitter;
		(void)p;
	}

	// 更新処理（毎フレーム）
	virtual void ApplyUpdate(ParticleEmitterInstance& emitter, Particle& p, float dt) {
		(void)emitter;
		(void)p;
		(void)dt;
	}

	// 描画時の処理（必要なら）
	virtual void ApplyRender(ParticleEmitterInstance& emitter, Particle& p) {
		(void)emitter;
		(void)p;
	}

private:
	bool enabled_ = true;
};
