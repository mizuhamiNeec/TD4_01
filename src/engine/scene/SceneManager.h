#pragma once

#include <engine/scene/SceneFactory.h>

/// @brief シーンマネージャークラス
class SceneManager {
public:
	explicit SceneManager(SceneFactory& factory);

	void ChangeScene(const std::string& name);

	void Update(float deltaTime) const;

	void Render() const;

	std::shared_ptr<BaseScene> GetCurrentScene() const;

private:
	SceneFactory&              mFactory;
	std::shared_ptr<BaseScene> mCurrentScene;
};
