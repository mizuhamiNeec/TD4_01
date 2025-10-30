#include <engine/Components/ColliderComponent/Base/ColliderComponent.h>

#include <engine/OldConsole/Console.h>

/// @brief オーバーラップ開始コールバックを登録します。
/// @param callback コールバック関数
void ColliderComponent::RegisterOnOverlapBeginCallback(
	const OnOverlapBegin& callback
) {
	mOnOverlapBeginCallbacks.emplace_back(callback);
}

/// @brief オーバーラップ終了コールバックを登録します。
/// @param callback コールバック関数
void ColliderComponent::RegisterOnOverlapEndCallback(
	const OnOverlapEnd& callback
) {
	mOnOverlapEndCallbacks.emplace_back(callback);
}
