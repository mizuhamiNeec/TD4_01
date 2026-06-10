#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>
#include <core/math/Vec3.h>

#include <engine/physics/core/Physics.h>

#include <string>
#include <string_view>

namespace Unnamed {
	class BaseKinematicCollisionResolver;
}

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

	/// @brief 発射開始位置をエンティティから設定（GolfBallStartPosComponent 付きエンティティ）
	/// @param entity GolfBallStartPosComponent がアタッチされたエンティティ
	void SetStartPosEntity(Unnamed::Entity* entity);

	/// @brief ターゲット位置をエンティティから設定（GolfBallEndPosComponent 付きエンティティ）
	/// @param entity GolfBallEndPosComponent がアタッチされたエンティティ
	void SetTargetPosEntity(Unnamed::Entity* entity);

	/// @brief 発射を開始（初速を逆算して計算）
	void Launch();

	/// @brief 速度に力を加える（衝撃波など外部からの力）
	/// @param force 加える力のベクトル
	void ApplyForce(const Vec3& force);

	/// @brief 穴の位置と吸い込み力を設定
	/// @param holePosition 穴の世界座標
	/// @param suckPower 吸い込み力（0.0～1.0、大きいほど強く）
	void SetHoleSuckPosition(const Vec3& holePosition, float suckPower);

	/// @brief 穴へ入ったことを確定し、落下演出へ移行
	/// @param holePosition 穴の世界座標
	void EnterHoleFall(const Vec3& holePosition);

	/// @brief 穴への吸い込み処理を無効化
	void ClearHoleSuckPosition();

	// -----------------------------------------------------------------------
	// 状態取得
	// -----------------------------------------------------------------------

	/// @brief 現在の位置を取得
	[[nodiscard]] Vec3 GetCurrentPosition() const;

	/// @brief 現在の速度を取得
	[[nodiscard]] Vec3 GetCurrentVelocity() const;

	/// @brief ボールが飛行中かを取得
	[[nodiscard]] bool IsInFlight() const;

	/// @brief 穴への吸い込み処理中かを取得
	[[nodiscard]] bool IsBeingSucked() const;

	/// @brief 経過時間を取得
	[[nodiscard]] float GetElapsedTime() const;

	/// @brief 発射後に地面へバウンドした回数を取得
	[[nodiscard]] int GetBounceCount() const;

	/// @brief 発射後に一度でもバウンドしたかを取得
	[[nodiscard]] bool HasBounced() const;

	/// @brief 一度でも穴の内側に入ったかを取得
	[[nodiscard]] bool HasEnteredHole() const;

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

	/// @brief 穴への吸い込み処理
	void UpdateHoleSuck(float deltaTime);

	// -----------------------------------------------------------------------
	// 状態管理（内部メソッド）
	// -----------------------------------------------------------------------

	/// @brief 着地判定（Y座標がターゲット高さ以下かを確認）
	/// @return 着地しているかのフラグ
	/// @reason フライト継続の判定に使用
	[[nodiscard]] bool CheckLanding() const;

	// -----------------------------------------------------------------------
	// 物理計算（内部メソッド）- バウンス・摩擦用
	// -----------------------------------------------------------------------

	/// @brief 地面衝突判定と反射を処理
	/// @reason ボールが地面に到達したときの物理的な反射を計算
	void HandleGroundCollision(float deltaTime);

	/// @brief 接地衝撃を反発速度へ変換
	/// @reason 座標地面と物理オブジェクト上面の衝突で反発処理を共通化する
	void ResolveGroundImpact(float impactSpeed, bool wasGrounded);

	/// @brief 物理衝突解決後にオブジェクト上面への着地を補正
	/// @reason SlideMove が下向き速度を潰した場合でも、着地時の反発を残す
	void HandlePostSlideGroundImpact(const Vec3& velocityBeforeSlide);

	/// @brief 地表摩擦を適用
	/// @reason 地面上での速度減衰を計算
	void ApplyFriction();

	/// @brief 保存済み GUID から開始/着弾マーカーを復元
	void ResolveSavedEntityReferences();

	/// @brief マーカー参照がある場合に設定値へ反映
	void SyncSetupFromReferencedEntities();

	/// @brief 現在位置を Transform と衝突形状へ反映
	void ApplyPositionToRuntime();

	/// @brief ターゲット周辺のランダムオフセットを再計算
	void RefreshTargetRandomOffset();

	// -----------------------------------------------------------------------
	// 物理
	// -----------------------------------------------------------------------	
	Unnamed::Physics::Engine*                                _physicsEngine = nullptr;
	
	/// 物理衝突解決用のリゾルバ
	std::unique_ptr<Unnamed::BaseKinematicCollisionResolver> mCollisionResolver;
	
	float _radius = 0.25f; // ボールの半径

	/// ボールの重さ。外力による速度変化を抑えるために使用
	float _mass = 4.0f;

	/// 外力で一度に増える上向き速度の上限
	float _maxExternalUpwardVelocity = 8.0f;

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
	// 物理パラメータ（バウンス・摩擦）
	// -----------------------------------------------------------------------

	/// 反発係数（0.0～1.0、地面衝突時の速度反転スケール）
	/// 理由：低いほどエネルギーを失い、バウンドが減衰する
	float _bounceDamping = 0.7f;

	/// バウンドとして扱う最小の縦方向衝突速度
	/// 理由：小さな接地でいつまでも跳ね続ける挙動を抑える
	float _minBounceVerticalSpeed = 0.35f;

	/// 地面衝突時の横方向速度減衰
	/// 理由：接地時に少しエネルギーを失わせ、軽すぎる跳ね返りを避ける
	float _groundCollisionDamping = 0.92f;

	/// 地表摩擦係数（0.0～1.0、毎フレームの速度減衰）
	/// 理由：地面上での横方向速度を徐々に減衰させる
	float _frictionCoefficient = 0.95f;

	/// 地面判定の高さ閾値（単位: units）
	/// 理由：Y <= この値で地面衝突と判定
	float _groundLevel = 0.0f;

	/// 停止判定の速度閾値（単位: units/sec）
	/// 理由：速度がこれ以下で完全に停止と判定
	float _stopVelocityThreshold = 0.01f;

	// -----------------------------------------------------------------------
	// 状態変数
	// -----------------------------------------------------------------------

	/// 現在位置（ワールド座標）
	Vec3 _position = Vec3(0.0f, 0.0f, 0.0f);

	/// 保存される発射位置
	Vec3 _startPoint = Vec3(0.0f, 0.0f, 0.0f);

	/// 現在速度
	Vec3 _velocity = Vec3(0.0f, 0.0f, 0.0f);

	/// ターゲット基準位置
	Vec3 _targetBase = Vec3(0.0f, 0.0f, 0.0f);

	/// ターゲットランダムオフセット（着地点のばらつき）
	Vec3 _targetRandomOffset = Vec3(0.0f, 0.0f, 0.0f);

	/// 経過時間（フライト開始からの時間）
	float _elapsedTime = 0.0f;

	/// 発射後に地面へバウンドした回数
	int _bounceCount = 0;

	/// フライト状態フラグ
	bool _bIsInFlight = false;

	/// 地面に接触しているかのフラグ
	/// 理由：摩擦を適用するかどうかの判定に使用
	bool _bIsGrounded = false;

	/// 衝撃波など、発射ターゲットとは独立した外力で動いているか
	bool _bIsExternalMotion = false;

	/// 吸い込み対象の穴の位置
	Vec3 _holeSuckPosition = Vec3(0.0f, 0.0f, 0.0f);

	/// 吸い込み力（0.0～1.0、大きいほど強く）
	float _holeSuckPower = 0.0f;

	/// 吸い込み中フラグ
	bool _bIsBeingSucked = false;

	/// 穴の内側に位置しているフラグ（地面衝突判定をスキップするため）
	bool _bIsInsideHole = false;

	/// 穴へ入った後の落下演出を継続するためのフラグ
	/// 理由：穴範囲から外れても ClearHoleSuckPosition() で落下を止めない
	bool _bHasEnteredHole = false;

	/// 起動後の初期設定反映が完了したか
	bool _bInitialSetupApplied = false;

	// -----------------------------------------------------------------------
	// エンティティ参照
	// -----------------------------------------------------------------------

	/// 発射位置を指定するエンティティ（GolfBallStartPosComponent 付き）
	/// 理由：外部から参照を受け取ってその位置を発射地点として使用
	Unnamed::Entity* _startPosEntity = nullptr;

	/// 着弾位置を指定するエンティティ（GolfBallEndPosComponent 付き）
	/// 理由：外部から参照を受け取ってその位置を着弾地点として使用
	Unnamed::Entity* _targetPosEntity = nullptr;

	/// 発射位置エンティティのGUID（JSON保存用）
	/// 理由：エンティティポインタは直接保存できないためGUIDで参照
	std::string _startPosEntityGuid = "";

	/// 着弾位置エンティティのGUID（JSON保存用）
	/// 理由：エンティティポインタは直接保存できないためGUIDで参照
	std::string _targetPosEntityGuid = "";
};

}
