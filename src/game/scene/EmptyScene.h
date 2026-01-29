#pragma once
#include <memory>

#include <engine/CubeMap/CubeMap.h>

#include <game/scene/base/BaseScene.h>

#include <runtime/physics/core/UPhysics.h>

#include "runtime/gui/UiDocument.h"

class D3D12;

/**
 * @brief 空のシーンクラス
 * @details テスト用の最小限のシーン実装
 */
class EmptyScene : public BaseScene {
public:
	/**
	 * @brief デストラクタ
	 */
	~EmptyScene() override;

	/**
	 * @brief シーンの初期化
	 */
	void Init() override;

	/**
	 * @brief シーンの更新
	 * @param deltaTime 前フレームからの経過時間
	 */
	void Update(float deltaTime) override;

	/**
	 * @brief シーンの描画
	 */
	void Render() override;

	/**
	 * @brief シーンの終了処理
	 */
	void Shutdown() override;

private:
	void BindUiCallbacks() const;

	D3D12*                                    mRenderer         = nullptr;
	bool                                      mGameLaunchQueued = false;
	std::shared_ptr<Unnamed::Gui::UiDocument> mActiveDocument;
};
