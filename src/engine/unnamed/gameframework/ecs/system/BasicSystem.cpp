#include "BasicSystem.h"

namespace Unnamed::ECS {
	static bool IsValidEntity(const Entity& e) {
		return e.id != kInvalidEntityId;
	}

	void TransformSystem::MarkLocalDirty(World& world, Entity entity) {
		if (!world.IsAlive(entity)) {
			return;
		}
		if (!world.Has<Transform>(entity)) {
			return;
		}

		Transform& transform = world.Get<Transform>(entity);
		transform.localDirty = true;
		transform.worldDirty = true;

		MarkWorldDirtyRecursive(world, entity);
	}

	void TransformSystem::MarkWorldDirtyRecursive(World& world, Entity entity) {
		if (!world.IsAlive(entity)) {
			return;
		}

		if (!world.Has<Transform>(entity)) {
			return;
		}

		Transform& transform = world.Get<Transform>(entity);
		transform.worldDirty = true;

		if (!world.Has<TransformHierarchy>(entity)) {
			return;
		}

		TransformHierarchy& hierarchy = world.Get<TransformHierarchy>(entity);
		Entity              child     = hierarchy.firstChild;
		while (IsValidEntity(child) && world.IsAlive(child)) {
			if (world.Has<Transform>(child)) {
				world.Get<Transform>(child).worldDirty = true;
			}

			if (world.Has<TransformHierarchy>(child)) {
				child = world.Get<TransformHierarchy>(child).nextSibling;
			} else {
				break; // 階層を持たない場合は終了
			}
		}
	}

	void TransformSystem::SetParent(
		World& world, const Entity child, const Entity parent
	) {
		if (!world.IsAlive(child) || !world.IsAlive(parent)) {
			return;
		}

		if (!world.Has<Transform>(child)) {
			world.Add<Transform>(child, Transform{});
		}

		if (!world.Has<Transform>(parent)) {
			world.Add<Transform>(parent, Transform{});
		}

		if (!world.Has<TransformHierarchy>(child)) {
			world.Add<TransformHierarchy>(child, TransformHierarchy{});
		}

		TransformHierarchy& childHierarchy =
			world.Get<TransformHierarchy>(child);

		// 親から引き剥がす
		if (IsValidEntity(childHierarchy.parent)) {
			DetachFromParent(world, child, childHierarchy);
		}

		// 新しい親にぶち込む
		AttachToParent(world, child, childHierarchy, parent);

		// 親が変わったのでワールド行列を更新
		MarkWorldDirtyRecursive(world, child);
	}

	void TransformSystem::ClearParent(World& world, Entity child) {
		if (!world.IsAlive(child)) {
			return;
		}
		if (!world.Has<TransformHierarchy>(child)) {
			return;
		}

		TransformHierarchy& childHierarchy = world.Get<TransformHierarchy>(
			child);
		if (!IsValidEntity(childHierarchy.parent)) {
			return;
		}

		DetachFromParent(world, child, childHierarchy);
		MarkWorldDirtyRecursive(world, child);
	}

	void TransformSystem::Update(World& world) {
	}

	Mat4 TransformSystem::BuildLocalMat(const Transform& transform) {
		return Mat4::Affine(
			transform.localScale,
			transform.localRotation,
			transform.localPosition
		);
	}

	void TransformSystem::UpdateWorldRecursive(
		World&      world,
		Entity      entity,
		const Mat4& parentWorld,
		bool        parentWorldDirty
	) {
	}

	void TransformSystem::DetachFromParent(
		World& world, Entity child, TransformHierarchy& childHierarchy
	) {
		Entity parent = childHierarchy.parent;
		if (!IsValidEntity(parent) || !world.IsAlive(parent)) {
			childHierarchy.parent      = Entity{};
			childHierarchy.prevSibling = Entity{};
			childHierarchy.nextSibling = Entity{};
			return;
		}

		if (!world.Has<TransformHierarchy>(parent)) {
			childHierarchy.parent      = Entity{};
			childHierarchy.prevSibling = Entity{};
			childHierarchy.nextSibling = Entity{};
			return;
		}

		TransformHierarchy& parentHierarchy = world.Get<TransformHierarchy>(
			parent);
		Entity prev = childHierarchy.prevSibling;
		Entity next = childHierarchy.nextSibling;

		if (IsValidEntity(prev) && world.IsAlive(prev) && world.Has<
			TransformHierarchy>(prev)) {
			world.Get<TransformHierarchy>(prev).nextSibling = next;
		} else {
			parentHierarchy.firstChild = next;
		}

		if (IsValidEntity(next) && world.IsAlive(next) && world.Has<
			TransformHierarchy>(next)) {
			world.Get<TransformHierarchy>(next).prevSibling = prev;
		}

		childHierarchy.parent      = Entity{};
		childHierarchy.prevSibling = Entity{};
		childHierarchy.nextSibling = Entity{};
	}

	void TransformSystem::AttachToParent(
		World&              world,
		Entity              child,
		TransformHierarchy& childHierarchy,
		Entity              parent
	) {
		if (!IsValidEntity(parent) || !world.IsAlive(parent)) {
			return;
		}

		// 親にTransformHierarchyコンポーネントがなければ追加
		if (!world.Has<TransformHierarchy>(parent)) {
			world.Add<TransformHierarchy>(parent, TransformHierarchy{});
		}

		TransformHierarchy& parentHierarchy = world.Get<TransformHierarchy>(
			parent);

		Entity oldFirst            = parentHierarchy.firstChild;
		parentHierarchy.firstChild = child;

		childHierarchy.parent      = parent;
		childHierarchy.prevSibling = Entity{};
		childHierarchy.nextSibling = oldFirst;

		if (
			IsValidEntity(oldFirst) &&
			world.IsAlive(oldFirst) &&
			world.Has<TransformHierarchy>(oldFirst)
		) {
			world.Get<TransformHierarchy>(oldFirst).prevSibling = child;
		}
	}

	void TransformSystem::ForEachChild(
		World&                                   world,
		Entity                                   parent,
		const std::function<void(Entity child)>& fn
	) {
		if (!world.Has<TransformHierarchy>(parent)) {
			return;
		}

		TransformHierarchy& parentHierarchy = world.Get<TransformHierarchy>(
			parent);
		Entity child = parentHierarchy.firstChild;
		while (IsValidEntity(child) && world.IsAlive(child)) {
			fn(child);

			if (!world.Has<TransformHierarchy>(child)) {
				break;
			}

			child = world.Get<TransformHierarchy>(child).nextSibling;
		}
	}
}
