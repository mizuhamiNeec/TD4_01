#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Particle/ParticleEmitterInstance.h"

struct Mat4;
class ParticleSystem;
// ===============================================
// 複数の ParticleSystem を保有・管理する
// 
// 責務：
//   - System の生成/検索/削除
//   - System の JSON 保存/読込
//   - System へのプリセット登録
//   - 既存 Emitter を System に紐づける
//   - 全 System の Update(dt) 駆動
// ===============================================
class ParticleSystemManager {
public:
	// ===== 生成・検索 =====

	/// System を 1 つ作成して保有する。同名があれば既存を返す
	ParticleSystem* Create(const std::string& systemName);

	/// 名前で System を検索（なければ nullptr）
	ParticleSystem* Find(const std::string& systemName);
	const ParticleSystem* Find(const std::string& systemName) const;

	/// System 名を変更
	/// @return 成功した場合 true、oldName が存在しないか newName が重複していたら false
	bool Rename(const std::string& oldName, const std::string& newName);

	/// 全 System を削除
	void ClearAll();

	// ===== 一覧取得 =====

	/// 全 System 名を取得
	std::vector<std::string> GetAllNames() const;

	// ===== プリセット登録 =====

	/// 指定 System に「使用するプリセット名」を登録（同名は無視）
	void RegisterPreset(const std::string& systemName, const std::string& presetName);

	/// 指定 System に登録されているプリセット名一覧を取得（なければ nullptr）
	const std::vector<std::string>* GetPresets(const std::string& systemName) const;

	// ===== Emitter の登録 =====

	/// 既に生成済みの EmitterInstance を System に紐づける
	/// @note Emitter の生成・所有は呼び出し側（ParticleManager）が行う
	void RegisterEmitter(const std::string& systemName,ParticleEmitterInstance* emitter,float startTime = 0.0f,float duration = -1.0f,bool autoPlay = true);

	// ===== System の Emit（再生開始） =====

	/// System を Emit（再生）する
	/// emitterFactory プリセット名から EmitterInstance を生成する関数
	void EmitSystem(const std::string& systemName,const Mat4& transform,const std::function<ParticleEmitterInstance* (const std::string& presetName,const Mat4& transform)>& emitterFactory);

	// ===== JSON 保存/読込 =====

	/// 1つの System を JSON に保存
	bool SaveToJson(const std::string& systemName,
		const std::string& directory = "Resources/ParticleSystem");

	/// 1つの System を JSON から読み込み
	bool LoadFromJson(const std::string& systemName,
		const std::string& directory = "Resources/ParticleSystem");

	/// ディレクトリ内の全 System JSON を読み込み
	void LoadAll(const std::string& directory = "Resources/ParticleSystem");

	// ===== Update 駆動 =====

	/// 全 System の Update(dt) を呼ぶ（時間制御）
	void UpdateAll(float dt);

private:
	std::vector<std::unique_ptr<ParticleSystem>> systems_;
};