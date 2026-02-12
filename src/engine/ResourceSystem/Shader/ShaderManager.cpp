#include <engine/OldConsole/Console.h>
#include <engine/ResourceSystem/Shader/Shader.h>
#include <engine/ResourceSystem/Shader/ShaderManager.h>

/// @brief シェーダの読み込み
/// @param name シェーダ名
/// @param vsPath 頂点シェーダのパス
/// @param psPath ピクセルシェーダのパス
/// @param gsPath ジオメトリシェーダのパス（省略可能）
/// @return シェーダへのポインタ（失敗した場合はnullptr）
Shader* ShaderManager::LoadShader(
	const std::string& name,
	const std::string& vsPath,
	const std::string& psPath,
	const std::string& gsPath
) {
	// パスを結合してキーにする
	const std::string filePath = vsPath + ";" + psPath + ";" + gsPath;

	// 既に読み込まれているシェーダがあればそれを返す
	if (mShaders.contains(filePath)) { return mShaders[filePath].get(); }

	// 新しいシェーダを作成
	if (auto shader = std::make_unique<Shader>(name, vsPath, psPath, gsPath)) {
		// シェーダの読み込みに成功したので登録
		mShaders[filePath] = std::move(shader);
		return mShaders[filePath].get();
	}

	Console::Print(
		"シェーダの読み込みに失敗しました: " + filePath + "\n",
		kConTextColorError,
		Channel::ResourceSystem
	);

	return nullptr;
}

/// @brief シェーダの取得
/// @param name シェーダ名
/// @return シェーダへのポインタ（存在しない場合はnullptr）
Shader* ShaderManager::GetShader(const std::string& name) {
	const auto it = mShaders.find(name);
	return it != mShaders.end() ? it->second.get() : nullptr;
}

/// @brief 初期化
void ShaderManager::Init() {
	Console::Print(
		"ShaderManager を初期化しています...\n",
		kConTextColorGray,
		Channel::ResourceSystem
	);
	mShaders.clear();
}

/// @brief 終了処理
void ShaderManager::Shutdown() {
	Console::Print(
		"ShaderManager を終了しています...\n", kConTextColorWait,
		Channel::ResourceSystem
	);

	// 個々のシェーダーインスタンスをクリーンアップ
	for (auto& [path, shader] : mShaders) {
		if (shader) {
			shader->Release();
			shader.reset();
		}
	}
	mShaders.clear();

	// 静的DXCリソースの解放
	Shader::ReleaseStaticResources();
}
