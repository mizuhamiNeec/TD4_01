#pragma once
#include "engine/unnamed/gameframework/ecs/ECSEntity.h"

#include "runtime/core/math/Math.h"

namespace Unnamed::ECS {
	struct Transform final {
		Vec3       localPosition = Vec3::zero;
		Quaternion localRotation = Quaternion::identity;
		Vec3       localScale    = Vec3::one;

		Mat4 localMat;
		Mat4 worldMat;

		bool localDirty = true;
		bool worldDirty = true;
	};

	struct TransformHierarchy final {
		Entity parent;
		Entity firstChild;
		Entity nextSibling;
		Entity prevSibling;
	};
}
