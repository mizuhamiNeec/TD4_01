#pragma once
#include <memory>
#include <vector>
#include <string>
#include "Particle.h"       // struct Particle { position, velocity, life,... }
#include "ParticlePreset.h" // struct ParticlePreset {...}
#include "ParticleModule.h" // class ParticleModule（モジュール基底クラス）

#include "core/math/Mat4.h"

class ParticleEmitterInstance
{
public:
	using PMPreset = ParticlePreset;

	ParticleEmitterInstance() = default;

	// ==============================
	// 初期化 & 基本情報
	// ==============================
	// ※プリセットへの「生ポインタ」を保持するだけ（所有権は ParticleManager）
	void Initialize(const PMPreset* preset);
	void SetTransform(const Mat4& t);
	const Mat4& GetTransform() const { return emitterTransform_; }

	// ==============================
	// 再生制御
	// ==============================
	void Play();      // ループ再生開始
	void Stop();      // 停止（今ある粒子はそのまま死ぬまで動く）
	void Reset();     // 全粒子クリア
	bool IsPlaying() const { return playing_; }

	// ==============================
	// 毎フレーム更新（シミュレーションだけ）
	// ==============================
	// ParticleManager から呼ぶのは基本これだけ
	void Update(float dt);

	// EmitOnce 相当：周囲から「一発だけ出したい」ときにも使える
	void SpawnParticles();

	// 内部用：寿命・移動・カーブ適用など
	void UpdateParticles(float dt);

	// ==============================
	// モジュール（Niagara の Module 相当）
	// ==============================
	// このエミッタが持つモジュール列（読み取り専用）。
	// エディタ表示やデバッグから参照できる。
	const std::vector<std::unique_ptr<ParticleModule>>& GetModules() const { return modules_; }

	// ==============================
	// 描画側から見るための情報（Read-only）
	// ==============================
	const PMPreset* GetPreset() const { return preset_; }

	// 型が分からない所でも使えるように Raw も用意
	const void* GetPresetRaw() const { return static_cast<const void*>(preset_); }

	// このエミッタが持っている粒子配列（描画側は読み取り専用）
	const std::vector<Particle>& GetParticles() const { return particles_; }

	// RenderModule の情報をそのまま返すヘルパ
	bool IsBillboard() const { return preset_ ? preset_->render.useBillboard : true; }
	bool IsFlipY()     const { return preset_ ? preset_->render.flipY : false; }
	bool IsLocalSpace() const { return preset_ ? preset_->emitterSpawn.useLocalSpace : false; }

private:
	// 標準モジュール列を生成して modules_ に詰める
	void BuildModules();

private:
	// Niagara の「モジュール」情報（定数データ）
	const PMPreset* preset_ = nullptr;

	// このエミッタに適用するモジュール列。
	// Emit 時／更新時にこの順番で ApplySpawn / ApplyUpdate される。
	std::vector<std::unique_ptr<ParticleModule>> modules_;

	// エミッタ自体の Transform（発生位置・向き）
	Mat4 emitterTransform_{};

	// 実行時の粒子状態
	std::vector<Particle> particles_;

	// Emit 制御用
	float emitTimer_ = 0.0f;
	bool  playing_ = true;
};
