#include "EngineComponentRegistration.h"

#include "core/ComponentRegistry.h"

#include "engine/unnamed/framework/components/CameraComponent.h"
#include "engine/unnamed/framework/components/SkyboxComponent.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/audio/AudioSourceComponent.h"
#include "engine/unnamed/framework/components/collider/StaticMeshColliderComponent.h"
#include "engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.h"
#include "engine/unnamed/framework/components/mesh/SkeletalMeshRendererComponent.h"
#include "engine/unnamed/framework/components/mesh/StaticMeshRendererComponent.h"
#include "engine/unnamed/framework/components/sequence/SequenceDirectorComponent.h"
#include "engine/unnamed/framework/components/ui/UiCanvasComponent.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	void RegisterDefaultEngineComponents(ComponentRegistry& componentRegistry) {
		const auto registerIfMissing = [&](auto typeTag) {
			using T = decltype(typeTag);
			const T probe{};
			const std::string_view stableName = probe.GetStableName();
			if (componentRegistry.IsRegistered(stableName)) {
				return;
			}

			const bool registered = componentRegistry.Register(
				stableName,
				[]() -> std::unique_ptr<BaseComponent> {
					return std::make_unique<T>();
				},
				probe.GetComponentName()
			);
			if (!registered) {
				Warning(
					"EngineComponentRegistration",
					"Failed to register engine component '{}'.",
					stableName
				);
			}
		};

		registerIfMissing(TransformComponent{});
		registerIfMissing(CameraComponent{});
		registerIfMissing(SkyboxComponent{});
		registerIfMissing(StaticMeshRendererComponent{});
		registerIfMissing(StaticMeshColliderComponent{});
		registerIfMissing(SkeletalMeshRendererComponent{});
		registerIfMissing(SkeletalAnimationComponent{});
		registerIfMissing(UiCanvasComponent{});
		registerIfMissing(AudioSourceComponent{});
		registerIfMissing(SequenceDirectorComponent{});
	}
}

