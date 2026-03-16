#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "interface/IMovementState.h"

namespace Unnamed {
	class ConsoleSystem;
	class BaseCharacterComponent;

	class GameMovementStateMachine {
	public:
		GameMovementStateMachine();
		~GameMovementStateMachine();

		void Init();
		void Tick(MovementContext& context, float deltaTime);
		void ChangeState(std::string stateName);
		void AddState(const std::shared_ptr<IMovementState>& state);

		[[nodiscard]] std::shared_ptr<IMovementState> GetCurrentState() const;

	private:
		ConsoleSystem* mConsole = nullptr;
		BaseCharacterComponent* mCharacterComponent = nullptr;

		std::unordered_map<std::string, std::shared_ptr<IMovementState>>
		mStates;
		std::shared_ptr<IMovementState> mCurrentState = nullptr;
	};
}
