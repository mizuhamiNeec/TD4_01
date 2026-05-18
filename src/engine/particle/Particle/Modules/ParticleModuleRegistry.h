#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

class ParticleModule;

// ===============================================
// ParticleModuleRegistry
//
// 「モジュール種別名 → 生成関数」を登録しておくレジストリ。
// JSON の "type" 文字列やエディタのメニューから、
// 対応する ParticleModule を生成（ファクトリ）する。
//
// UE5 Niagara の「モジュールを追加」できる仕組みの土台。
// 新しいモジュールを足したいときは Register() を呼ぶだけでよい。
//
// シングルトン。組み込みモジュールはコンストラクタで自動登録される。
// ===============================================
class ParticleModuleRegistry {
public:
	using Factory = std::function<std::unique_ptr<ParticleModule>()>;

	// 唯一のインスタンスを取得する
	static ParticleModuleRegistry& Get();

	// モジュール種別を登録する（同名が既にあれば上書き）
	void Register(const std::string& typeName, Factory factory);

	// 種別名からモジュールを生成する（未登録なら nullptr）
	std::unique_ptr<ParticleModule> Create(const std::string& typeName) const;

	// 指定種別が登録済みか
	bool IsRegistered(const std::string& typeName) const;

	// 既定スタックの種別順。
	// プリセットにモジュールリストが無い（旧データ等）ときに使う。
	const std::vector<std::string>& GetDefaultOrder() const { return defaultOrder_; }

	// 登録済み種別名の一覧（エディタの「モジュール追加」メニュー用）
	std::vector<std::string> GetRegisteredTypes() const;

private:
	// 組み込みモジュールをここで登録する
	ParticleModuleRegistry();

	// 登録順を保持するため vector で持つ（unordered_map は順序が不定）
	std::vector<std::pair<std::string, Factory>> factories_;

	// 既定スタックの種別順
	std::vector<std::string> defaultOrder_;
};
