#include "ParticleSystem.h"

ParticleSystem::ParticleSystem(const std::string& name)
    : name_(name)
{
    // Transform の初期値（必要なら調整）
    transform_.scale = { 1.0f, 1.0f, 1.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = { 0.0f, 0.0f, 0.0f };
}

ParticleEmitterInstance* ParticleSystem::AddEmitter(ParticleEmitterInstance* emitter,float startTime,float duration,bool autoPlay)
{
    if (!emitter) {
        return nullptr;
    }
    EmitterEntry entry;
    entry.emitter = emitter;
    entry.startTime = startTime;
    entry.duration = duration;
    entry.autoPlay = autoPlay;
    entry.playedOnce = false;
    entry.stoppedOnce = false;

    emitters_.push_back(entry);
    return emitter;
}

void ParticleSystem::AddPresetName(const std::string& presetName)
{
    if (presetName.empty()) {
        return;
    }

    // 重複チェック
    for (const auto& n : presetNames_) {
        if (n == presetName) {
            return;
        }
    }
    presetNames_.push_back(presetName);
}

// ====== 再生制御 ======

void ParticleSystem::Play()
{
    time_ = 0.0f;
    playing_ = true;

    // すべての Emitter を初期状態に戻してから、時間に応じて再生させる
    for (auto& e : emitters_) {
        e.playedOnce = false;
        e.stoppedOnce = false;
        if (e.emitter) {
            e.emitter->Reset();
            e.emitter->Stop(); // startTime に達するまでは止めておく
        }
    }
}

void ParticleSystem::Stop()
{
    playing_ = false;

    // 必要なら Emitter も止める
    for (auto& e : emitters_) {
        if (e.emitter) {
            e.emitter->Stop();
        }
    }
}

void ParticleSystem::Reset()
{
    time_ = 0.0f;

    for (auto& e : emitters_) {
        e.playedOnce = false;
        e.stoppedOnce = false;
        if (e.emitter) {
            e.emitter->Reset();
            e.emitter->Stop();
        }
    }
}

// ====== Update ======

void ParticleSystem::Update(float dt)
{
    if (!playing_) {
        return;
    }

    time_ += dt;

    // 再生開始時間に達した Emitter に Play をかけるだけ
    // ※ Emitter の Update(dt)（シミュレーション）は ParticleManager 側で行う
    for (auto& e : emitters_) {
        if (!e.emitter) {
            continue;
        }

		// ===== Play 判定 =====
		// まだ Play していない Emitter を、startTime に達したら Play する
		if (!e.playedOnce) {
			if (time_ >= e.startTime) {
				if (e.autoPlay) {
					e.emitter->Reset();
					e.emitter->Play();
				}
				e.playedOnce = true;
			}
		}

		// ===== Stop 判定 =====
		// duration > 0 の場合、startTime + duration 経過で自動停止
		// すでに Play 済みで、まだ Stop していない Emitter のみ対象
		if (e.playedOnce && !e.stoppedOnce && e.duration > 0.0f) {
			const float stopTime = e.startTime + e.duration;
			if (time_ >= stopTime) {
				e.emitter->Stop();
				e.stoppedOnce = true;
			}
		}
	}

	// ===== ここから System 自体のループ処理 =====
	if (duration_ > 0.0f && time_ >= duration_) {
		if (loop_) {
			Play();
		}
		else {
			playing_ = false;
		}
	}
}

ParticleSystem::EmitterEntry* ParticleSystem::FindEntryByPresetName(const std::string& presetName)
{
	for (auto& e : emitters_) {
		if (!e.emitter) continue;

		// emitter から preset 名を取得
		const auto* preset = e.emitter->GetPreset();
		if (!preset) continue;

		if (preset->name == presetName) {
			return &e;
		}
	}
	return nullptr;
}
