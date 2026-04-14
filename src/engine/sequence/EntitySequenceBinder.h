#pragma once

#include <string_view>

#include "SequenceBinder.h"

namespace Unnamed {
	class Scene;
	class Entity;
	class TransformComponent;
	class UiCanvasComponent;

	class EntitySequenceBinder final : public ISequenceBinder {
	public:
		explicit EntitySequenceBinder(Scene* scene = nullptr);

		void SetScene(Scene* scene);
		[[nodiscard]] Scene* GetScene() const;

		[[nodiscard]] bool SupportsTarget(
			SEQUENCE_BINDING_TARGET target
		) const override;
		bool ApplyValue(const SequenceBinding& binding, float value) override;

	private:
		[[nodiscard]] bool ApplyTransformProperty(
			TransformComponent& transform,
			std::string_view    propertyPath,
			float               value
		) const;
		[[nodiscard]] bool ApplyUiCanvasProperty(
			UiCanvasComponent& canvas,
			std::string_view   propertyPath,
			float              value
		) const;
		[[nodiscard]] bool ApplyKnownComponentProperty(
			Entity&           entity,
			std::string_view  componentStableName,
			std::string_view  propertyPath,
			float             value
		) const;

		Scene* mScene = nullptr;
	};
}
