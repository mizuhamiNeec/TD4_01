#pragma once
#include <functional>

#include "engine/unnamed/gameframework/ecs/World.h"
#include "engine/unnamed/gameframework/ecs/component/BasicComponent.h"

namespace Unnamed::ECS {
	class TransformSystem {
	public:
		static void MarkLocalDirty(World& world, Entity entity);
		static void MarkWorldDirtyRecursive(World& world, Entity entity);

		static void SetParent(World& world, Entity child, Entity parent);
		static void ClearParent(World& world, Entity child);

		static void Update(World& world);

	private:
		static Mat4 BuildLocalMat(const Transform& transform);

		static void UpdateWorldRecursive(
			World&      world, Entity     entity,
			const Mat4& parentWorld, bool parentWorldDirty
		);
		static void DetachFromParent(
			World&              world, Entity child,
			TransformHierarchy& childHierarchy
		);
		static void AttachToParent(
			World&              world, Entity          child,
			TransformHierarchy& childHierarchy, Entity parent
		);

		static void ForEachChild(
			World&                                   world, Entity parent,
			const std::function<void(Entity child)>& fn
		);
	};
}
