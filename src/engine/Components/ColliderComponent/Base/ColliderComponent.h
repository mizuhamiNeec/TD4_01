#pragma once
#include <functional>

#include <engine/Components/base/Component.h>
#include <engine/uphysics/PhysicsTypes.h>

#include <runtime/core/math/Math.h>

/// @brief すべてのコライダーコンポーネントの基底クラス
class ColliderComponent : public Component {
public:
	using OnOverlapBegin = std::function<void(UPhysics::Hit&)>;
	using OnOverlapEnd   = std::function<void(UPhysics::Hit*)>;

	void Update(float deltaTime) override = 0;
	void DrawInspectorImGui() override = 0;

	void RegisterOnOverlapBeginCallback(const OnOverlapBegin& callback);
	void RegisterOnOverlapEndCallback(const OnOverlapEnd& callback);

	virtual void CheckCollision(const ColliderComponent* other) const = 0;

protected:
	std::vector<OnOverlapBegin> mOnOverlapBeginCallbacks;
	std::vector<OnOverlapEnd>   mOnOverlapEndCallbacks;
};
