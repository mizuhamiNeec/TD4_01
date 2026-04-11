#include "EntitySequenceBinder.h"

#include <algorithm>
#include <cmath>

#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include "engine/unnamed/framework/components/ui/UiCanvasComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"

namespace Unnamed {
	EntitySequenceBinder::EntitySequenceBinder(Scene* scene)
		: mScene(scene) {}

	void EntitySequenceBinder::SetScene(Scene* scene) {
		mScene = scene;
	}

	Scene* EntitySequenceBinder::GetScene() const {
		return mScene;
	}

	bool EntitySequenceBinder::SupportsTarget(
		const SEQUENCE_BINDING_TARGET target
	) const {
		return target == SEQUENCE_BINDING_TARGET::ENTITY;
	}

	bool EntitySequenceBinder::ApplyValue(
		const SequenceBinding& binding,
		const float            value
	) {
		if (
			binding.target != SEQUENCE_BINDING_TARGET::ENTITY ||
			!mScene ||
			binding.entity.entityGuid == 0
		) {
			return false;
		}

		Entity* entity = mScene->FindEntity(binding.entity.entityGuid);
		if (!entity) {
			return false;
		}

		return ApplyKnownComponentProperty(
			*entity,
			binding.entity.componentStableName,
			binding.entity.propertyPath,
			value
		);
	}

	bool EntitySequenceBinder::ApplyTransformProperty(
		TransformComponent&     transform,
		const std::string_view propertyPath,
		const float            value
	) const {
		if (propertyPath == "position.x") {
			Vec3 position = transform.GetPosition();
			position.x = value;
			transform.SetPosition(position);
			return true;
		}
		if (propertyPath == "position.y") {
			Vec3 position = transform.GetPosition();
			position.y = value;
			transform.SetPosition(position);
			return true;
		}
		if (propertyPath == "position.z") {
			Vec3 position = transform.GetPosition();
			position.z = value;
			transform.SetPosition(position);
			return true;
		}

		if (propertyPath == "scale.x") {
			Vec3 scale = transform.GetScale();
			scale.x = value;
			transform.SetScale(scale);
			return true;
		}
		if (propertyPath == "scale.y") {
			Vec3 scale = transform.GetScale();
			scale.y = value;
			transform.SetScale(scale);
			return true;
		}
		if (propertyPath == "scale.z") {
			Vec3 scale = transform.GetScale();
			scale.z = value;
			transform.SetScale(scale);
			return true;
		}

		if (propertyPath == "rotation.euler.x") {
			Vec3 euler = transform.GetRotation().ToEulerDegrees();
			euler.x = value;
			transform.SetRotation(Quaternion::EulerDegrees(euler));
			return true;
		}
		if (propertyPath == "rotation.euler.y") {
			Vec3 euler = transform.GetRotation().ToEulerDegrees();
			euler.y = value;
			transform.SetRotation(Quaternion::EulerDegrees(euler));
			return true;
		}
		if (propertyPath == "rotation.euler.z") {
			Vec3 euler = transform.GetRotation().ToEulerDegrees();
			euler.z = value;
			transform.SetRotation(Quaternion::EulerDegrees(euler));
			return true;
		}

		if (propertyPath == "rotation.x") {
			Quaternion rotation = transform.GetRotation();
			rotation.x = value;
			rotation.Normalize();
			transform.SetRotation(rotation);
			return true;
		}
		if (propertyPath == "rotation.y") {
			Quaternion rotation = transform.GetRotation();
			rotation.y = value;
			rotation.Normalize();
			transform.SetRotation(rotation);
			return true;
		}
		if (propertyPath == "rotation.z") {
			Quaternion rotation = transform.GetRotation();
			rotation.z = value;
			rotation.Normalize();
			transform.SetRotation(rotation);
			return true;
		}
		if (propertyPath == "rotation.w") {
			Quaternion rotation = transform.GetRotation();
			rotation.w = value;
			rotation.Normalize();
			transform.SetRotation(rotation);
			return true;
		}

		return false;
	}

	bool EntitySequenceBinder::ApplyUiCanvasProperty(
		UiCanvasComponent&      canvas,
		const std::string_view propertyPath,
		const float            value
	) const {
		if (propertyPath == "pixelSize.x") {
			Vec2 pixelSize = canvas.GetPixelSize();
			pixelSize.x = value;
			canvas.SetPixelSize(pixelSize);
			return true;
		}
		if (propertyPath == "pixelSize.y") {
			Vec2 pixelSize = canvas.GetPixelSize();
			pixelSize.y = value;
			canvas.SetPixelSize(pixelSize);
			return true;
		}
		if (propertyPath == "worldSize.x") {
			Vec2 worldSize = canvas.GetWorldSize();
			worldSize.x = value;
			canvas.SetWorldSize(worldSize);
			return true;
		}
		if (propertyPath == "worldSize.y") {
			Vec2 worldSize = canvas.GetWorldSize();
			worldSize.y = value;
			canvas.SetWorldSize(worldSize);
			return true;
		}
		if (propertyPath == "sortKey") {
			canvas.SetSortKey(static_cast<int32_t>(std::lround(value)));
			return true;
		}
		if (propertyPath == "receiveInput") {
			canvas.SetReceiveInput(value >= 0.5f);
			return true;
		}
		if (propertyPath == "spaceMode") {
			int mode = static_cast<int>(std::lround(value));
			mode = std::clamp(mode, 0, 2);
			canvas.SetSpaceMode(static_cast<UI_CANVAS_SPACE_MODE>(mode));
			return true;
		}
		if (propertyPath == "billboardDepthMode") {
			int mode = static_cast<int>(std::lround(value));
			mode = std::clamp(mode, 0, 1);
			canvas.SetBillboardDepthMode(
				static_cast<UI_CANVAS_BILLBOARD_DEPTH_MODE>(mode)
			);
			return true;
		}
		return false;
	}

	bool EntitySequenceBinder::ApplyKnownComponentProperty(
		Entity&                 entity,
		const std::string_view  componentStableName,
		const std::string_view  propertyPath,
		const float             value
	) const {
		if (componentStableName == "engine.Transform") {
			if (auto* transform = entity.GetComponent<TransformComponent>()) {
				return ApplyTransformProperty(*transform, propertyPath, value);
			}
			return false;
		}

		if (componentStableName == "engine.UiCanvas") {
			if (auto* canvas = entity.GetComponent<UiCanvasComponent>()) {
				return ApplyUiCanvasProperty(*canvas, propertyPath, value);
			}
			return false;
		}

		return false;
	}
}
