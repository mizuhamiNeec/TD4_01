#pragma once
#include <functional>
#include <memory>
#include <string>

#include <game/scene/base/BaseScene.h>

/// @brief シーンファクトリークラス
class SceneFactory {
public:
	template <typename T>
	void RegisterScene(const std::string& name) {
		mSceneCreators[name] = []() -> std::shared_ptr<BaseScene> {
			return std::make_shared<T>();
		};
	}

	std::shared_ptr<BaseScene> CreateScene(const std::string& name) {
		auto it = mSceneCreators.find(name);
		if (it != mSceneCreators.end()) { return it->second(); }
		return nullptr;
	}

private:
	std::unordered_map<std::string, std::function<std::shared_ptr<BaseScene>()>>
	mSceneCreators;
};
