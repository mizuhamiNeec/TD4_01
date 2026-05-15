#include "ParticleEmitterInstance.h"
#include <algorithm>
#include <random>

// 重力加速度（ParticleManager 側の挙動に合わせる）
static constexpr float kGravity = -9.81f;

static float RandomRange(float minValue, float maxValue)
{
	// 1つの乱数エンジンを使い回す
	static std::mt19937 s_rng(std::random_device{}());

	// ユーザーが min/max を逆に入れても落ちないようにする
	if (maxValue < minValue) {
		std::swap(minValue, maxValue);
	}

	// min == max の場合は、その値をそのまま返す
	if (minValue == maxValue) {
		return minValue;
	}

	std::uniform_real_distribution<float> dist(minValue, maxValue);
	return dist(s_rng);
}


// 便利用：内部で使うエイリアス
using PMPreset = ParticlePreset;

void ParticleEmitterInstance::Initialize(const PMPreset* preset)
{
	preset_ = preset;
	particles_.clear();
	emitTimer_ = 0.0f;
	playing_ = true;
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

	const auto& spawn = preset_->emitterSpawn;
	const auto& pSpawn = preset_->particleSpawn;
	const auto& u = preset_->particleUpdate;

	for (uint32_t i = 0; i < spawn.count; ++i) {
		Particle p{};

		// Local Space の場合: 原点(0,0,0)からの相対座標
		// World Space の場合: エミッタのワールド座標が基準
		Vec3 pos = spawn.useLocalSpace ? Vec3{ 0,0,0 } : emitterTransform_.GetTranslate();

		// --- EmitterSpawn 側のランダム位置（既存） ---
		if (spawn.useRandomPosition) {
			float rx = RandomRange(-1.0f, 1.0f);
			float ry = RandomRange(-1.0f, 1.0f);
			float rz = RandomRange(-1.0f, 1.0f);
			pos.x += rx; pos.y += ry; pos.z += rz;
		}

		// --- ParticleSpawn.initialOffset のランダム ---
		Vec3 offset = pSpawn.initialOffset;
		if (pSpawn.initialOffsetRandom.useRandom) {
			offset.x = RandomRange(
				pSpawn.initialOffsetRandom.minValue.x,
				pSpawn.initialOffsetRandom.maxValue.x);
			offset.y = RandomRange(
				pSpawn.initialOffsetRandom.minValue.y,
				pSpawn.initialOffsetRandom.maxValue.y);
			offset.z = RandomRange(
				pSpawn.initialOffsetRandom.minValue.z,
				pSpawn.initialOffsetRandom.maxValue.z);
		}
		pos.x += offset.x;
		pos.y += offset.y;
		pos.z += offset.z;

		p.position = pos;

		// --- 初期スケール（ランダム対応） ---
		Vec3 scale = pSpawn.initialScale;
		if (pSpawn.initialScaleRandom.useRandom) {
			scale.x = RandomRange(
				pSpawn.initialScaleRandom.minValue.x,
				pSpawn.initialScaleRandom.maxValue.x);
			scale.y = RandomRange(
				pSpawn.initialScaleRandom.minValue.y,
				pSpawn.initialScaleRandom.maxValue.y);
			scale.z = RandomRange(
				pSpawn.initialScaleRandom.minValue.z,
				pSpawn.initialScaleRandom.maxValue.z);
		}
		p.scale = scale;

		// --- 初期回転（ランダム対応） ---
		Vec3 rot = pSpawn.initialRotate;
		if (pSpawn.initialRotateRandom.useRandom) {
			rot.x = RandomRange(
				pSpawn.initialRotateRandom.minValue.x,
				pSpawn.initialRotateRandom.maxValue.x);
			rot.y = RandomRange(
				pSpawn.initialRotateRandom.minValue.y,
				pSpawn.initialRotateRandom.maxValue.y);
			rot.z = RandomRange(
				pSpawn.initialRotateRandom.minValue.z,
				pSpawn.initialRotateRandom.maxValue.z);
		}
		p.rotation = rot;

		// カラー初期値
		p.color = preset_->render.color;
		p.initialScale = p.scale;
		p.initialColor = p.color;

		// ========= ParticleUpdate 側 =========

		// --- 速度（ランダム対応） ---
		Vec3 vel = u.velocity;
		if (u.velocityRandom.useRandom) {
			vel.x = RandomRange(
				u.velocityRandom.minValue.x, u.velocityRandom.maxValue.x);
			vel.y = RandomRange(
				u.velocityRandom.minValue.y, u.velocityRandom.maxValue.y);
			vel.z = RandomRange(
				u.velocityRandom.minValue.z, u.velocityRandom.maxValue.z);
		}
		p.velocity = vel;

		// --- 回転速度（ランダム対応） ---
		Vec3 rotSpd = u.rotationSpeed;
		if (u.rotationRandom.useRandom) {
			rotSpd.x = RandomRange(
				u.rotationRandom.minValue.x, u.rotationRandom.maxValue.x);
			rotSpd.y = RandomRange(
				u.rotationRandom.minValue.y, u.rotationRandom.maxValue.y);
			rotSpd.z = RandomRange(
				u.rotationRandom.minValue.z, u.rotationRandom.maxValue.z);
		}
		p.rotationSpeed = rotSpd;

		// --- スケール速度（ランダム対応） ---
		Vec3 sclSpd = u.scaleSpeed;
		if (u.scaleRandom.useRandom) {
			sclSpd.x = RandomRange(
				u.scaleRandom.minValue.x, u.scaleRandom.maxValue.x);
			sclSpd.y = RandomRange(
				u.scaleRandom.minValue.y, u.scaleRandom.maxValue.y);
			sclSpd.z = RandomRange(
				u.scaleRandom.minValue.z, u.scaleRandom.maxValue.z);
		}
		p.scaleSpeed = sclSpd;

		// 寿命
		p.life = 0.0f;
		p.maxLife = u.lifeTime;
		p.active = true;

		particles_.push_back(p);
	}
}

void ParticleEmitterInstance::UpdateParticles(float dt)
{
	if (!preset_) { return; }

	const auto& spawn = preset_->emitterSpawn;
	const auto& updateM = preset_->particleUpdate;
	const auto& renderM = preset_->render;

	// 1) 自動 Emit 制御
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

	// 2) 既存粒子の更新
	for (auto& p : particles_) {
		if (!p.active) { continue; }

		// 寿命更新
		p.life += dt;
		if (p.life >= p.maxLife) {
			p.active = false;
			continue;
		}

		float age = p.life / p.maxLife; // 0～1

		// --- 移動 ---
		p.position.x += p.velocity.x * dt;
		p.position.y += p.velocity.y * dt;
		p.position.z += p.velocity.z * dt;

		// 重力
		if (updateM.useGravity) {
			p.velocity.y += kGravity * dt;  // kGravity = -9.81f
		}

		// --- 回転 ---
		p.rotation.x += p.rotationSpeed.x * dt;
		p.rotation.y += p.rotationSpeed.y * dt;
		p.rotation.z += p.rotationSpeed.z * dt;

		// --- スケール ---
		p.scale.x += p.scaleSpeed.x * dt;
		p.scale.y += p.scaleSpeed.y * dt;
		p.scale.z += p.scaleSpeed.z * dt;

		// カーブスケール（倍率）
		if (updateM.scaleCurve.enabled && !updateM.scaleCurve.keys.empty()) {
			float s = updateM.scaleCurve.Evaluate(age);
			p.scale.x = p.initialScale.x * s;
			p.scale.y = p.initialScale.y * s;
			p.scale.z = p.initialScale.z * s;
		}

		// =============================
		// 色：グラデーション + 時間カーブ + 強さカーブ
		// =============================
		{
			// 1) グラデーション用の t を決める（時間カーブ）
			float gradT = age;
			if (renderM.gradientTimeCurve.enabled && !renderM.gradientTimeCurve.keys.empty()) {
				gradT = renderM.gradientTimeCurve.Evaluate(age);
			}
			gradT = std::clamp(gradT, 0.0f, 1.0f);

			// 2) グラデーションから「ベース色」を取得
			Vec4 col = p.initialColor;
			if (renderM.colorGradient.enabled && !renderM.colorGradient.keys.empty()) {
				col = renderM.colorGradient.Evaluate(gradT);
			}

			// 3) colorCurve で色の強さ/αを時間でコントロール
			if (renderM.colorCurve.enabled && !renderM.colorCurve.keys.empty()) {
				float c = renderM.colorCurve.Evaluate(age);
				col.x *= c;
				col.y *= c;
				col.z *= c;
				col.w *= c;
			}

			p.color = col;
		}


	}

	// 3) 死んだ粒子をまとめて削除
	particles_.erase(
		std::remove_if(particles_.begin(), particles_.end(),
			[](const Particle& p) { return !p.active; }),
		particles_.end()
	);
}