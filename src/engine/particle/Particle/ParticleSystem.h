#pragma once
#include <string>
#include <vector>
#include "Transform.h"
#include "ParticleEmitterInstance.h"

// ===============================================
// ParticleSystem
// 1つの「エフェクト」を表すクラス。
// - System 名（Editor でユーザーが付ける名前）
// - System 全体の Transform
// - 複数の ParticleEmitterInstance を保持
// - 「この System に登録されているプリセット名一覧」を持つ
//
// ※ 所有権は ParticleManager 側の
//   std::vector<std::unique_ptr<ParticleEmitterInstance>>
//   にあります。ここは生ポインタで参照するだけ。
//
// ※ Draw は持たず、「再生タイミング制御（Play/Stop）」だけ行う。
//    実際のシミュレーション（Update(dt)）と描画は ParticleManager が担当。
// ===============================================
class ParticleSystem
{
public:
	struct EmitterEntry {
		ParticleEmitterInstance* emitter = nullptr;
		float startTime = 0.0f;          // System time_ 基準でいつ Play するか
		float duration = -1.0f;          // 発火してから何秒で Stop するか
		//   -1.0f 以下なら「停止しない」
		bool  autoPlay = true;           // startTime に達したら自動で Play するか
		bool  playedOnce = false;        // 一度 Play したかどうか
		bool  stoppedOnce = false;       // 一度 Stop したかどうか
	};

	using EmitterList = std::vector<EmitterEntry>;

	explicit ParticleSystem(const std::string& name = "");

	// ----- System 名 -----
	const std::string& GetName() const { return name_; }
	void SetName(const std::string& n) { name_ = n; }

	// ----- System 全体の Transform（必要なら使用） -----
	const Transform& GetTransform() const { return transform_; }
	void SetTransform(const Transform& t) { transform_ = t; }

	// ----- 所属エミッタ一覧 -----
	const EmitterList& GetEmitters() const { return emitters_; }
	EmitterList& GetEmitters() { return emitters_; }

	// ParticleManager 側で生成済みの EmitterInstance を登録するだけ
	// startTime: 再生開始時間（System 再生開始からの秒数）
	// autoPlay : time_ >= startTime になったときに自動で Play するか
	ParticleEmitterInstance* AddEmitter(
		ParticleEmitterInstance* emitter,
		float startTime = 0.0f,
		float duration = -1.0f,
		bool autoPlay = true
	);

	// ----- System に紐付いているプリセット名一覧 -----
	const std::vector<std::string>& GetPresetNames() const { return presetNames_; }

	// 同じ名前があれば追加しない
	void AddPresetName(const std::string& presetName);
	void ClearPresetNames() { presetNames_.clear(); }

	// ====== 再生制御 ======
	void Play();   // time_ を 0 にして再生開始
	void Stop();   // System の時間を止め、必要なら Emitter も Stop
	void Reset();  // time_ を 0 に戻し、Emitter も Reset + Stop
	bool IsPlaying() const { return playing_; }

	// System 全体の長さ＆ループ設定
	void SetDuration(float d) { duration_ = d; }   // d <= 0 なら「長さ未設定＝ループ判定しない」
	float GetDuration() const { return duration_; }

	void SetLoop(bool loop) { loop_ = loop; }
	bool IsLoop() const { return loop_; }

	// ====== 毎フレーム更新 ======
	// ・System の time_ を進める
	// ・startTime に達した Emitter に対して Play() を呼ぶ など
	// ※ Emitter の Update(dt) はここでは呼ばない
	void Update(float dt);

	// ===== JSON 保存/読込用 =====

	// プリセット名から対応する EmitterEntry を探す（なければ nullptr）
	// JSON 読込時に「プリセット名 → startTime/duration」をセットするのに使う
	EmitterEntry* FindEntryByPresetName(const std::string& presetName);

	// プリセット名と startTime/duration を指定して「予約エントリ」を追加
	// JSON 読込時に使う。実際の emitter は EmitSystem() の時に作成される
	struct PendingEmitterSetting {
		std::string presetName;
		float startTime = 0.0f;
		float duration = -1.0f;
		bool  autoPlay = true;
	};

	// JSON から読んだ設定をここに溜めておく
	// EmitSystem() 初回の Emitter 作成時にこの設定が参照される
	std::vector<PendingEmitterSetting>& GetPendingSettings() { return pendingSettings_; }
	const std::vector<PendingEmitterSetting>& GetPendingSettings() const { return pendingSettings_; }
	void ClearPendingSettings() { pendingSettings_.clear(); }

private:
	std::string name_;
	Transform   transform_{};

	// 実行時にぶら下がるエミッタ
	EmitterList emitters_;

	// 「この System で使うプリセット名」の一覧
	std::vector<std::string> presetNames_;

	// System 内部時間
	float time_ = 0.0f;
	bool  playing_ = false;

	// System の長さとループ設定
	float duration_ = 0.0f;  // 0 のままなら「ループ判定しない」
	bool  loop_ = false;

	// JSON 読込時の「未作成エミッタの設定」一覧
	// EmitSystem() で Emitter を作成するときに参照し、startTime/duration を設定する
	std::vector<PendingEmitterSetting> pendingSettings_;
};