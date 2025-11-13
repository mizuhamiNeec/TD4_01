#pragma once
#include "engine/Components/base/Component.h"

#include "runtime/core/math/Vec3.h"

class RotateComponent : public Component {
public:
	void OnAttach(Entity& owner) override;

	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;


	void SetRotationRate(const Vec3& rotationRate) {
		mRotationRate = rotationRate;
	}

	[[nodiscard]] const Vec3& GetRotationRate() const noexcept {
		return mRotationRate;
	}

	void SetRotationEnabled(const bool enabled) {
		mRotationEnabled = enabled;
	}

	[[nodiscard]] bool IsRotationEnabled() const noexcept {
		return mRotationEnabled;
	}

	void SetPitchEnabled(const bool enabled) {
		mPitchEnabled = enabled;
	}

	void SetYawEnabled(const bool enabled) {
		mYawEnabled = enabled;
	}

	void SetRollEnabled(bool enabled) {
		mRollEnabled = enabled;
	}

private:
	Vec3 mRotationRate = Vec3::zero; // 回転速度（度/秒）

	bool mRotationEnabled = true; // 回転を有効にするか
	bool mPitchEnabled    = true; // X軸（ピッチ）回転を有効にするか
	bool mYawEnabled      = true; // Y軸（ヨー）回転を有効にするか
	bool mRollEnabled     = true; // Z軸（ロール）回転を有効にするか

	class SceneComponent* mTransform = nullptr;
};
