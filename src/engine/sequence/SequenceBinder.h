#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "engine/tween/TweenTypes.h"

namespace Unnamed {
	enum class SEQUENCE_BINDING_TARGET : uint8_t {
		ENTITY = 0,
		UI     = 1,
	};

	struct SequenceKeyframe {
		float     time  = 0.0f;
		float     value = 0.0f;
		EASE_TYPE ease  = EASE_TYPE::LINEAR;
	};

	struct EntitySequenceBinding {
		uint64_t    entityGuid = 0;
		std::string componentStableName;
		std::string propertyPath;
	};

	struct UiSequenceBinding {
		uint64_t    canvasEntityGuid = 0;
		std::string widgetName;
		std::string componentType;
		std::string propertyPath;
	};

	struct SequenceBinding {
		SEQUENCE_BINDING_TARGET target = SEQUENCE_BINDING_TARGET::ENTITY;
		EntitySequenceBinding   entity = {};
		UiSequenceBinding       ui     = {};
	};

	struct SequenceTrack {
		std::string                 name;
		SequenceBinding             binding = {};
		std::vector<SequenceKeyframe> keyframes;
	};

	class ISequenceBinder {
	public:
		virtual ~ISequenceBinder() = default;

		[[nodiscard]] virtual bool SupportsTarget(
			SEQUENCE_BINDING_TARGET target
		) const = 0;
		virtual bool ApplyValue(
			const SequenceBinding& binding, float value
		) = 0;
	};
}
