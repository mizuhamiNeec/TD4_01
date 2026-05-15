#pragma once
#include "ParticlePreset.h"

#include <string>
#include <unordered_map>
#include <vector>

// ===============================================
// プリセット（ParticlePreset）の保存・読込・検索を担当
// 
// 責務：
//   - プリセットの JSON 保存/読込
//   - プリセットの名前検索
//   - プリセット一覧の管理
// ===============================================
class ParticlePresetLibrary {
public:
	// ===== 保存・読込 =====

	/// プリセットを JSON に保存する
	/// return 成功した場合 true
	bool SaveToJson(const ParticlePreset& preset,
		const std::string& directory = "Resources/Particle");

	/// JSON からプリセットを読み込む（presetName.json を読む）
	/// param outPreset 読み込み結果がここに格納される
	/// return 成功した場合 true
	bool LoadFromJson(const std::string& presetName,
		ParticlePreset& outPreset,
		const std::string& directory = "Resources/Particle");

	/// ディレクトリ内のすべてのプリセットを読み込む
	/// 内部マップ presets_ にも登録される
	void LoadAll(const std::string& directory = "Resources/Particle");

	// ===== プリセット管理 =====

	/// プリセットを内部マップに追加（メモリのみ。JSONには保存しない）
	void Add(const ParticlePreset& preset);

	/// プリセットを内部マップから削除
	void Remove(const std::string& name);

	/// 名前でプリセットを検索（編集用）
	ParticlePreset* Find(const std::string& name);
	const ParticlePreset* Find(const std::string& name) const;

	// ===== 一覧 =====

	/// 全プリセット名を取得
	std::vector<std::string> GetAllNames() const;

	/// 内部マップへの直接アクセス
	std::unordered_map<std::string, ParticlePreset>& GetAll() { return presets_; }
	const std::unordered_map<std::string, ParticlePreset>& GetAll() const { return presets_; }

	// ===== 編集状態 =====

	/// エディタで現在編集中のプリセット名
	const std::string& GetCurrentEditingName() const { return currentEditingName_; }
	void SetCurrentEditingName(const std::string& name) { currentEditingName_ = name; }

private:
	// プリセット保有マップ（name -> ParticlePreset）
	std::unordered_map<std::string, ParticlePreset> presets_;

	// 現在編集中のプリセット名
	std::string currentEditingName_;
};