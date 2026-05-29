#include "ParticleEmitterInstance.h"

#include "Modules/ParticleModuleRegistry.h"

#include <algorithm>

// 便利用：内部で使うエイリアス
using PMPreset = ParticlePreset;

void ParticleEmitterInstance::BuildModules()
{
	// モジュール列を、プリセットのモジュールスタック定義から構築し直す。
	modules_.clear();
	if (!preset_) { return; }

	ParticleModuleRegistry& registry = ParticleModuleRegistry::Get();

	// プリセットに modules リストが無い場合（旧データ・コード生成プリセット等）は
	// レジストリの既定スタック順で構築する。
	if (preset_->modules.empty()) {
		for (const auto& type : registry.GetDefaultOrder()) {
			if (auto m = registry.Create(type)) {
				modules_.push_back(std::move(m));
			}
		}
		return;
	}

	// modules リストの「種類・順序・有効状態」をそのまま反映する。
	// ※ ApplySpawn / ApplyUpdate はこの順番で呼ばれる。
	//   既定スタックは Lifetime が先頭で、後続モジュールが死んだ粒子をスキップする。
	for (const auto& slot : preset_->modules) {
		auto m = registry.Create(slot.type);
		if (!m) {
			// 未知のモジュール種別は無視する（前方互換のため落とさない）
			continue;
		}
		m->SetEnabled(slot.enabled);
		modules_.push_back(std::move(m));
	}
}

void ParticleEmitterInstance::Initialize(const PMPreset* preset)
{
	preset_ = preset;
	particles_.clear();
	emitTimer_ = 0.0f;
	playing_ = true;

	// プリセットを差し替えたタイミングでモジュール列を構築する
	BuildModules();
}

void ParticleEmitterInstance::SetTransform(const Mat4& t)
{
	emitterTransform_ = t;
}

void ParticleEmitterInstance::Play()
{
	playing_ = true;
	emitTimer_ = 0.0f;
}

void ParticleEmitterInstance::Stop()
{
	playing_ = false;
}

void ParticleEmitterInstance::Reset()
{
	particles_.clear();
	emitTimer_ = 0.0f;
}

void ParticleEmitterInstance::Update(float dt)
{
	if (!preset_) { return; }

	// 今の設計では Spawn ロジックは UpdateParticles 内部に入れてもいいし、
	// ここから SpawnParticles() を呼んでも OK
	UpdateParticles(dt);
}

void ParticleEmitterInstance::SpawnParticles()
{
	if (!preset_) { return; }

	// 発生数ぶん粒子を作り、初期化は各モジュールの ApplySpawn に委譲する
	const uint32_t count = preset_->emitterSpawn.count;
	for (uint32_t i = 0; i < count; ++i) {
		Particle p{};
		for (auto& m : modules_) {
			if (m->IsEnabled()) {
				m->ApplySpawn(*this, p);
			}
		}
		particles_.push_back(p);
	}
}

void ParticleEmitterInstance::UpdateParticles(float dt)
{
	if (!preset_) { return; }

	const auto& spawn = preset_->emitterSpawn;

	// 1) 自動 Emit 制御（発生数・頻度・繰り返しはエミッタ自身の責務）
	if (playing_) {
		emitTimer_ += dt;

		if (spawn.frequency <= 0.0f) {
			// frequency 0 → 毎フレーム Emit
			SpawnParticles();
		}
		else {
			while (emitTimer_ >= spawn.frequency) {
				SpawnParticles();
				emitTimer_ -= spawn.frequency;

				if (!spawn.repeat) {
					playing_ = false;
					break;
				}
			}
		}
	}

	// 2) 既存粒子の更新（寿命・移動・回転・スケール・色は各モジュールに委譲）
	//    LifetimeModule が先頭で寿命切れを判定し active=false にするため、
	//    後続モジュールは ApplyUpdate 内で死んだ粒子をスキップする。
	for (auto& p : particles_) {
		if (!p.active) { continue; }
		for (auto& m : modules_) {
			if (m->IsEnabled()) {
				m->ApplyUpdate(*this, p, dt);
			}
		}
	}

	// 3) 死んだ粒子をまとめて削除
	particles_.erase(
		std::remove_if(particles_.begin(), particles_.end(),
			[](const Particle& p) { return !p.active; }),
		particles_.end()
	);
}
