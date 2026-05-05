#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>
#include <core/math/Vec3.h>

namespace MyGame {

/// @brief ゴルフボールの放物運動を管理するコンポーネント
/// 
/// 責務分離設計：
/// - 物理計算部: 重力・風・速度制限による基礎運動
/// - 誘導計算部: ターゲット追従機能（時間ベース）
/// - 状態管理部: 初期化・更新・終了判定
/// 
/// 使用方法：
/// 1. SetStartPoint() で開始位置を設定
/// 2. SetTargetPoint() でターゲット位置を設定
/// 3. OnTick() で毎フレーム更新（自動）
/// 4. GetCurrentPosition() で現在位置を取得
class GolfBallComponent : public Unnamed::BaseComponent {
public:
	// -----------------------------------------------------------------------
	// ライフサイクル
	// -----------------------------------------------------------------------

	/// コンポーネントがアタッチされたときに呼び出される
	void OnAttached() override;

	/// 毎フレーム更新（放物運動の計算を実行）
	void OnTick(float deltaTime) override;

	/// コンポーネントがデタッチされたときに呼び出される
	void OnDetached() override;

	// -----------------------------------------------------------------------
	// セットアップ
	// -----------------------------------------------------------------------

	/// @brief 発射開始位置を設定
	/// @param startPos 開始位置（ワールド座標）
	void SetStartPoint(const Vec3& startPos);

	/// @brief ターゲット位置を設定（着地ランダム位置を自動計算）
	/// @param targetPos ターゲット基準点（ワールド座標）
	void SetTargetPoint(const Vec3& targetPos);

	/// @brief 発射を開始（初速を逆算して計算）
	void Launch();

	// -----------------------------------------------------------------------
	// 状態取得
	// -----------------------------------------------------------------------

	/// @brief 現在の位置を取得
	[[nodiscard]] Vec3 GetCurrentPosition() const;

	/// @brief 現在の速度を取得
	[[nodiscard]] Vec3 GetCurrentVelocity() const;

	/// @brief ボールが飛行中かを取得
	[[nodiscard]] bool IsInFlight() const;

	/// @brief 経過時間を取得
	[[nodiscard]] float GetElapsedTime() const;

	// -----------------------------------------------------------------------
	// BaseComponent override
	// -----------------------------------------------------------------------

	[[nodiscard]] std::string_view GetStableName() const override;
	[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
	void DrawInspectorImGui() override;
#endif

	/// コンポーネントの値を読み込む際に使用されます
	void Deserialize(const Unnamed::JsonReader& reader) override;

	/// コンポーネントの値を書き込む際に使用されます
	void Serialize(Unnamed::JsonWriter& writer) const override;

private:
	// -----------------------------------------------------------------------
	// 物理計算（内部メソッド）
	// -----------------------------------------------------------------------

	/// @brief 物理ベースの更新（重力・風・位置更新）
	/// @param deltaTime フレーム時間
	/// @reason 基礎となる物理挙動を分離することで、誘導補正との混同を避ける
	void UpdatePhysics(float deltaTime);

	/// @brief 速度をクランプ（上限制限）
	/// @reason 物理計算後に速度を制限し、不自然な加速を防止
	void ClampVelocity();

	// -----------------------------------------------------------------------
	// 誘導計算（内部メソッド）
	// -----------------------------------------------------------------------

	/// @brief 誘導補正を適用（ターゲットへの収束）
	/// @param deltaTime フレーム時間
	/// @reason 物理挙動と分離して、ホーミング機能を段階的に効果させる
	void ApplyHoming(float deltaTime);

	/// @brief 現在時刻における誘導強度を計算
	/// @return 誘導強度（0.0～1.0）
	/// @reason 時間ベースで徐々に誘導を有効化し、不自然な急転換を防止
	[[nodiscard]] float GetHomingAlpha() const;

	/// @brief 誘導方向を計算（ボール → ターゲット）
	/// @return 正規化された誘導方向ベクトル
	/// @reason 現在位置からターゲットへ向かう方向を導出
	[[nodiscard]] Vec3 CalcHomingDirection() const;

	// -----------------------------------------------------------------------
	// 状態管理（内部メソッド）
	// -----------------------------------------------------------------------

	/// @brief 着地判定（Y座標がターゲット高さ以下かを確認）
	/// @return 着地しているかのフラグ
	/// @reason フライト継続の判定に使用
	[[nodiscard]] bool CheckLanding() const;

	// -----------------------------------------------------------------------
	// 放物運動パラメータ
	// -----------------------------------------------------------------------

	/// 重力加速度（単位: units/sec²）
	float _gravity = 9.81f;

	/// 到達予定時間（単位: sec）
	float _flightTime = 2.0f;

	/// 着地ランダム半径（単位: units）
	float _randomRadius = 1.0f;

	/// 風ベクトル（加速度として毎フレーム加算）
	Vec3 _wind = Vec3(0.0f, 0.0f, 0.0f);

	/// 捕捉（ホーミング）強度（0.0～1.0）
	float _homingStrength = 1.0f;

	/// 捕捉開始時間（単位: sec）
	float _homingStartTime = 0.5f;

	/// 捕捉終了時間（単位: sec）
	float _homingEndTime = 1.5f;

	/// 速度上限クランプ値（単位: units/sec）
	float _maxSpeedClamp = 50.0f;

	// -----------------------------------------------------------------------
	// 状態変数
	// -----------------------------------------------------------------------

	/// 現在位置（ワールド座標）
	Vec3 _position = Vec3(0.0f, 0.0f, 0.0f);

	/// 現在速度
	Vec3 _velocity = Vec3(0.0f, 0.0f, 0.0f);

	/// ターゲット基準位置
	Vec3 _targetBase = Vec3(0.0f, 0.0f, 0.0f);

	/// ターゲットランダムオフセット（着地点のばらつき）
	Vec3 _targetRandomOffset = Vec3(0.0f, 0.0f, 0.0f);

	/// 経過時間（フライト開始からの時間）
	float _elapsedTime = 0.0f;

	/// フライト状態フラグ
	bool _bIsInFlight = false;
};

}
