#include <engine/SceneManager/SceneManager.h>

/// @brief コンストラクタ
/// @param factory シーンファクトリーへの参照
SceneManager::SceneManager(SceneFactory& factory) : mFactory(factory) {}

/// @brief シーンを変更します
/// @param name シーン名
void SceneManager::ChangeScene(const std::string& name) {
	if (std::shared_ptr<BaseScene> newScene = mFactory.CreateScene(name)) {
		if (mCurrentScene) { mCurrentScene->Shutdown(); }
		mCurrentScene = newScene;
		mCurrentScene->Init();
	}
}

/// @brief シーンを更新します
/// @param deltaTime 前フレームからの経過時間（秒）
void SceneManager::Update(const float deltaTime) const {
	if (mCurrentScene) { mCurrentScene->Update(deltaTime); }
}

/// @brief シーンをレンダリングします
void SceneManager::Render() const {
	if (mCurrentScene) { mCurrentScene->Render(); }
}

/// @brief 現在のシーンを取得します
std::shared_ptr<BaseScene> SceneManager::GetCurrentScene() const {
	return mCurrentScene;
}
