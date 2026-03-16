#pragma once

#include "game/core/components/character/GameMovementComponent.h"

namespace Unnamed {
	/// @brief パルクール向けの拡張移動コンポーネントです。
	class ParkourMovementComponent final : public GameMovementComponent {
	public:
		~ParkourMovementComponent() override;

		void OnAttached() override;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	protected:
		void RegisterMovementStates(
			GameMovementStateMachine& stateMachine
		) override;
	};
}
