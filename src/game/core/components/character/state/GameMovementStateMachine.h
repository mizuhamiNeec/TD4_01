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

		/// @brief 状態機械で利用するサービス参照を初期化します。
		void Init();

		/// @brief 現在状態を1フレーム更新します。
		void Tick(MovementContext& context, float deltaTime);

		/// @brief 指定した状態IDへ遷移します。
		void ChangeState(const std::string& stateName);

		/// @brief 状態を登録します。既存IDと重複した場合は上書きします。
		void AddState(const std::shared_ptr<IMovementState>& state);

		[[nodiscard]] std::shared_ptr<IMovementState> GetCurrentState() const;

	private:
		ConsoleSystem* mConsole = nullptr;
		BaseCharacterComponent* mCharacterComponent = nullptr;
		bool mHasReportedMissingConsole = false;

		std::unordered_map<std::string, std::shared_ptr<IMovementState>>
		mStates;
		std::shared_ptr<IMovementState> mCurrentState = nullptr;
	};
}
