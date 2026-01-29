#pragma once

#include <engine/Components/base/Component.h>
#include <runtime/core/math/Math.h>

/// @brief カメラコンポーネント
/// @details カメラです。
class CameraComponent : public Component {
public:
	~CameraComponent() override;

	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;
	void Render(ID3D12GraphicsCommandList* commandList) override;

	void DrawInspectorImGui() override;

	float GetFovVertical() const;
	void  SetFovVertical(float newFovVertical);

	float GetZNear() const;
	void  SetNearZ(float newNearZ);

	float GetZFar() const;
	void  SetFarZ(float newFarZ);

	Mat4 GetViewProjMat();
	Mat4 GetViewMat();
	Mat4 GetProjMat();

	float GetAspectRatio();
	void  SetAspectRatio(float newAspectRatio);
	void  SetViewMat(const Mat4& mat4);

private:
	float mFov         = 90.0f * Math::deg2Rad;
	float mAspectRatio = 0.0f;
	float mZNear       = 0.01f;
	float mZFar        = 0x61A8p0f;

	Mat4 mWorldMat;
	Mat4 mViewMat;
	Mat4 mProjMat;
	Mat4 mViewProjMat;
};
