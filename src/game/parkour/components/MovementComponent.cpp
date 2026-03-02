#include "MovementComponent.h"

#include <algorithm>
#include <array>
#include <imgui.h>

#include "CameraRotatorComponent.h"

#include "core/ComponentRegistry.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"
#include "core/math/Math.h"

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/UEntity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/input/UInputSystem.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/UWorld.h"

namespace Unnamed {
	namespace {
		float ReadFloatOr(
			const JsonReader& reader, const char* key, const float fallback
		) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetFloat() : fallback;
		}

		Vec3 ReadVec3Or(
			const JsonReader& reader, const char* key, const Vec3& fallback
		) {
			const JsonReader value = reader[key];
			if (!value.Valid() || value.Size() < 3) { return fallback; }
			return Vec3(
				value[0].GetFloat(),
				value[1].GetFloat(),
				value[2].GetFloat()
			);
		}
	}

	void MovementComponent::OnAttached() {
		mInput = ServiceLocator::Get<UInputSystem>();
	}

	void MovementComponent::PrePhysicsTick(const float) {}

	void MovementComponent::OnTick(float deltaTime) {
		auto* transform = GetTransform();
		if (!transform) { return; }
		const UWorld* world = UWorld::GetTickingWorld();
		if (!world || !world->IsGameSimulationEnabled()) {
			mReplayInputActive    = false;
			mWasJumpHeldLastFrame = false;
			return;
		}

		if (mSpeedBoostRemaining > 0.0f) {
			mSpeedBoostRemaining = std::max(
				0.0f, mSpeedBoostRemaining - deltaTime
			);
			if (mSpeedBoostRemaining == 0.0f) { mSpeedBoostMultiplier = 1.0f; }
		}
		if (mBlinkCooldownRemaining > 0.0f) {
			mBlinkCooldownRemaining = std::max(
				0.0f, mBlinkCooldownRemaining - deltaTime
			);
		}

		if (mSpeedVaulting) {
			mVaultProgress = std::clamp(mVaultProgress, 0.0f, 1.0f);
			const Vec3 a   = Math::Lerp(
				mVaultStartPos, mVaultApexPos, mVaultProgress
			);
			const Vec3 b = Math::Lerp(
				mVaultApexPos, mVaultEndPos, mVaultProgress
			);
			transform->SetPosition(Math::Lerp(a, b, mVaultProgress));
			return;
		}

		const InputFrame input         = SampleInput();
		const Vec3       wishDir       = BuildWishDirection(input.moveInput);
		const float      targetSpeedHu = (
			                                 input.wishCrouch ?
				                                 mCrouchSpeedHu :
				                                 mMoveSpeedHu
		                                 ) * mSpeedBoostMultiplier;

		if (mGrounded && !mSliding) {
			ApplyFriction(mFriction, deltaTime);
			Accelerate(wishDir, targetSpeedHu, mGroundAccelHu, deltaTime);
			if (input.wishJump && !mWasJumpHeldLastFrame) {
				mVelocity.y = Math::HtoM(mJumpVelocityHu);
				mGrounded   = false;
			}
			if (input.wishCrouch) { TryStartSlide(); }
		} else if (!mWallRunning) {
			Accelerate(wishDir, targetSpeedHu, mAirAccelHu, deltaTime);
		}

		if (mSliding) { UpdateSlide(deltaTime, input); }

		TryBlink(input);
		if (!mGrounded && !mSliding) {
			TryStartWallrun();
			UpdateWallrun(deltaTime, input);
		}

		if (!mGrounded) {
			const float gravityScale = mWallRunning ?
				                           mWallrunGravityScale :
				                           1.0f;
			mVelocity += Vec3::down * Math::HtoM(mGravityHu) * gravityScale *
				deltaTime;
		}

		ResolveMovement(mVelocity * deltaTime);
		UpdateGrounding();
		mWasJumpHeldLastFrame = input.wishJump;
	}

	void MovementComponent::PostPhysicsTick(float) {
		const UWorld* world = UWorld::GetTickingWorld();
		if (!world || !world->IsGameSimulationEnabled()) {
			mReplayInputActive = false;
			ClearLiveInputOverride();
			return;
		}
		if (mReplayInputActive) { mReplayInputActive = false; }
		if (mLiveInputOverrideActive) { ClearLiveInputOverride(); }
	}

	void MovementComponent::Deserialize(const JsonReader& reader) {
		mWidthHu        = ReadFloatOr(reader, "widthHu", mWidthHu);
		mHeightHu       = ReadFloatOr(reader, "heightHu", mHeightHu);
		mCrouchHeightHu = ReadFloatOr(
			reader, "crouchHeightHu", mCrouchHeightHu
		);
		mMoveSpeedHu    = ReadFloatOr(reader, "moveSpeedHu", mMoveSpeedHu);
		mJumpVelocityHu = ReadFloatOr(
			reader, "jumpVelocityHu", mJumpVelocityHu
		);
		mGravityHu       = ReadFloatOr(reader, "gravityHu", mGravityHu);
		mRespawnPosition = ReadVec3Or(
			reader, "respawnPosition", mRespawnPosition
		);
	}

	void MovementComponent::Serialize(JsonWriter& writer) const {
		writer.Key("widthHu");
		writer.Write(mWidthHu);
		writer.Key("heightHu");
		writer.Write(mHeightHu);
		writer.Key("crouchHeightHu");
		writer.Write(mCrouchHeightHu);
		writer.Key("moveSpeedHu");
		writer.Write(mMoveSpeedHu);
		writer.Key("jumpVelocityHu");
		writer.Write(mJumpVelocityHu);
		writer.Key("gravityHu");
		writer.Write(mGravityHu);
		writer.Key("respawnPosition");
		writer.BeginArray();
		writer.Write(mRespawnPosition.x);
		writer.Write(mRespawnPosition.y);
		writer.Write(mRespawnPosition.z);
		writer.EndArray();
	}

#ifdef _DEBUG
	void MovementComponent::DrawInspectorImGui() {
		ImGui::Checkbox("Grounded", &mGrounded);
		ImGui::Checkbox("Sliding", &mSliding);
		ImGui::Checkbox("WallRunning", &mWallRunning);
		ImGui::DragFloat3("Velocity", &mVelocity.x, 0.01f);
	}
#endif

	void MovementComponent::Initialize(
		UPhysics::Engine* const       physics,
		CameraRotatorComponent* const cameraRotator
	) {
		mPhysics       = physics;
		mCameraRotator = cameraRotator;
	}

	void MovementComponent::SetReplayFrame(const ReplayUserCmdFrame& frame) {
		mReplayFrame       = frame;
		mReplayInputActive = true;
		SetReplayVaultState(
			frame.hasVaultState && frame.isSpeedVaulting,
			frame.vaultProgress,
			Vec3(frame.vaultStartX, frame.vaultStartY, frame.vaultStartZ),
			Vec3(frame.vaultApexX, frame.vaultApexY, frame.vaultApexZ),
			Vec3(frame.vaultEndX, frame.vaultEndY, frame.vaultEndZ)
		);
	}

	void MovementComponent::ClearReplayFrame() { mReplayInputActive = false; }

	void MovementComponent::SetLiveInputOverride(
		const Vec2& moveInput,
		const bool  wishJump,
		const bool  wishCrouch,
		const bool  wishBlink
	) {
		mLiveInputOverride.moveInput  = moveInput;
		mLiveInputOverride.wishJump   = wishJump;
		mLiveInputOverride.wishCrouch = wishCrouch;
		mLiveInputOverride.wishBlink  = wishBlink;
		mLiveInputOverrideActive      = true;
	}

	void MovementComponent::ClearLiveInputOverride() {
		mLiveInputOverrideActive = false;
		mLiveInputOverride       = {};
	}

	void MovementComponent::SetRespawnPosition(const Vec3& respawnPosition) {
		mRespawnPosition = respawnPosition;
	}

	Vec3 MovementComponent::GetRespawnPosition() const noexcept {
		return mRespawnPosition;
	}

	void MovementComponent::ApplyJumpPadBoost(const float velocityHu) {
		mVelocity.y = Math::HtoM(velocityHu);
		mGrounded   = false;
	}

	void MovementComponent::ApplySpeedBoost(
		const float multiplier, const float durationSec
	) {
		if (multiplier <= 1.0f || durationSec <= 0.0f) { return; }
		mSpeedBoostMultiplier = std::max(mSpeedBoostMultiplier, multiplier);
		mSpeedBoostRemaining  = std::max(mSpeedBoostRemaining, durationSec);
	}

	void MovementComponent::TeleportTo(const Vec3& feetPosition) {
		if (auto* transform = GetTransform()) {
			transform->SetPosition(feetPosition);
		}
	}

	void MovementComponent::SetReplayVaultState(
		const bool  isSpeedVaulting,
		const float vaultProgress,
		const Vec3& vaultStartPos,
		const Vec3& vaultApexPos,
		const Vec3& vaultEndPos
	) {
		mSpeedVaulting = isSpeedVaulting;
		mVaultProgress = vaultProgress;
		mVaultStartPos = vaultStartPos;
		mVaultApexPos  = vaultApexPos;
		mVaultEndPos   = vaultEndPos;
	}

	Vec3 MovementComponent::GetVelocity() const noexcept { return mVelocity; }

	void MovementComponent::SetVelocity(const Vec3& velocity) noexcept {
		mVelocity = velocity;
	}

	Vec3 MovementComponent::GetFeetPosition() const noexcept {
		const auto* transform = GetTransform();
		return transform ? transform->Position() : Vec3::zero;
	}

	Vec3 MovementComponent::GetHeadPosition() const noexcept {
		return GetFeetPosition() + Vec3::up * Math::HtoM(
			       (mGrounded && mSliding) ? mCrouchHeightHu : mHeightHu
		       );
	}

	bool MovementComponent::IsGrounded() const noexcept { return mGrounded; }
	bool MovementComponent::IsSliding() const noexcept { return mSliding; }

	bool MovementComponent::IsWallRunning() const noexcept {
		return mWallRunning;
	}

	bool MovementComponent::IsSpeedVaulting() const noexcept {
		return mSpeedVaulting;
	}

	float MovementComponent::GetVaultProgress() const noexcept {
		return mVaultProgress;
	}

	Vec3 MovementComponent::GetVaultStartPos() const noexcept {
		return mVaultStartPos;
	}

	Vec3 MovementComponent::GetVaultApexPos() const noexcept {
		return mVaultApexPos;
	}

	Vec3 MovementComponent::GetVaultEndPos() const noexcept {
		return mVaultEndPos;
	}

	PlayerAabb MovementComponent::BuildWorldAabb() const noexcept {
		const float heightHu = (mGrounded && mSliding) ?
			                       mCrouchHeightHu :
			                       mHeightHu;
		return {
			.center = GetFeetPosition() + Vec3::up *
			          Math::HtoM(heightHu * 0.5f),
			.halfSize = Vec3(
				Math::HtoM(mWidthHu * 0.5f),
				Math::HtoM(heightHu * 0.5f),
				Math::HtoM(mWidthHu * 0.5f)
			),
		};
	}

	TransformComponent* MovementComponent::GetTransform() const {
		UEntity* owner = GetOwner();
		return owner ? owner->GetComponent<TransformComponent>() : nullptr;
	}

	MovementComponent::InputFrame MovementComponent::SampleInput() const {
		InputFrame frame = {};
		if (mReplayInputActive) {
			frame.moveInput.x = mReplayFrame.moveX;
			frame.moveInput.y = mReplayFrame.moveY;
			frame.wishJump    = (mReplayFrame.buttons & ReplayButton_Jump) != 0;
			frame.wishCrouch  = (mReplayFrame.buttons & ReplayButton_Crouch) !=
			                    0;
			frame.wishBlink = (mReplayFrame.buttons & ReplayButton_Blink) != 0;
			return frame;
		}

		if (mLiveInputOverrideActive) { return mLiveInputOverride; }

		if (!mInput) { return frame; }
		frame.moveInput.x =
			(mInput->IsHeld("moveright") ? 1.0f : 0.0f) +
			(mInput->IsHeld("moveleft") ? -1.0f : 0.0f);
		frame.moveInput.y =
			(mInput->IsHeld("forward") ? 1.0f : 0.0f) +
			(mInput->IsHeld("back") ? -1.0f : 0.0f);
		frame.wishJump   = mInput->IsHeld("jump");
		frame.wishCrouch = mInput->IsHeld("duck");
		frame.wishBlink  = mInput->IsPressed("attack2");

		Msg(
			"MovementComponent",
			" moveInput: ({:.2f}, {:.2f}), wishJump: {}, wishCrouch: {}, wishBlink: {}",
			frame.moveInput.x, frame.moveInput.y, frame.wishJump,
			frame.wishCrouch, frame.wishBlink
		);

		return frame;
	}

	Vec3 MovementComponent::BuildWishDirection(const Vec2& moveInput) const {
		if (moveInput.SqrLength() <= 1e-6f) { return Vec3::zero; }

		float yawDegrees = 0.0f;
		if (mCameraRotator) {
			yawDegrees = mCameraRotator->GetLookAnglesDegrees().y;
		}

		const Mat4 yawMatrix = Mat4::RotateY(yawDegrees * Math::deg2Rad);
		Vec3       forward   = yawMatrix.GetForward();
		forward.y            = 0.0f;
		forward.Normalize();
		Vec3 right = Vec3::up.Cross(forward).Normalized();
		Vec3 wish  = forward * moveInput.y + right * moveInput.x;
		wish.y     = 0.0f;
		wish.Normalize();
		return wish;
	}

	Unnamed::Box MovementComponent::BuildPhysicsBoxAt(
		const Vec3& feetPosition
	) const noexcept {
		const PlayerAabb aabb = BuildWorldAabb();
		return {
			.center   = feetPosition + Vec3::up * aabb.halfSize.y,
			.halfSize = aabb.halfSize,
		};
	}

	void MovementComponent::Accelerate(
		const Vec3  wishDir,
		const float wishSpeedHu,
		const float accelHu,
		const float deltaTime
	) {
		if (wishDir.SqrLength() <= 1e-6f) { return; }

		Vec3 horizontalVelocity    = mVelocity;
		horizontalVelocity.y       = 0.0f;
		const float currentSpeedHu = Math::MtoH(
			horizontalVelocity.Dot(wishDir)
		);
		const float addSpeedHu = wishSpeedHu - currentSpeedHu;
		if (addSpeedHu <= 0.0f) { return; }

		const float accelAmountHu = std::min(
			accelHu * wishSpeedHu * deltaTime,
			addSpeedHu
		);
		mVelocity += wishDir * Math::HtoM(accelAmountHu);
	}

	void MovementComponent::ApplyFriction(
		const float amount, const float deltaTime
	) {
		Vec3 horizontalVelocity = mVelocity;
		horizontalVelocity.y    = 0.0f;
		const float speed       = Math::MtoH(horizontalVelocity.Length());
		if (speed <= 1e-4f) { return; }

		const float drop     = speed * amount * deltaTime;
		const float newSpeed = std::max(0.0f, speed - drop);
		if (newSpeed <= 1e-4f) {
			mVelocity.x = 0.0f;
			mVelocity.z = 0.0f;
			return;
		}

		const float ratio = newSpeed / speed;
		mVelocity.x       *= ratio;
		mVelocity.z       *= ratio;
	}

	void MovementComponent::ResolveMovement(const Vec3& delta) {
		auto* transform = GetTransform();
		if (!transform) { return; }
		if (!mPhysics || delta.SqrLength() <= 1e-10f) {
			transform->SetPosition(transform->Position() + delta);
			return;
		}

		Vec3 currentPosition = transform->Position();
		Vec3 remaining       = delta;
		for (int iteration = 0; iteration < 3; ++iteration) {
			const float distance = remaining.Length();
			if (distance <= 1e-5f) { break; }

			const Vec3    direction = remaining / distance;
			UPhysics::Hit hit       = {};
			if (!mPhysics->BoxCast(
				BuildPhysicsBoxAt(currentPosition), direction, distance, &hit
			)) {
				currentPosition += remaining;
				break;
			}

			const float safeDistance = std::max(0.0f, hit.t - Math::HtoM(1.0f));
			currentPosition += direction * safeDistance;
			const Vec3 slideNormal = hit.normal.Normalized();
			remaining = remaining - slideNormal * remaining.Dot(slideNormal);
			mVelocity = mVelocity - slideNormal * mVelocity.Dot(slideNormal);
		}

		transform->SetPosition(currentPosition);
	}

	void MovementComponent::UpdateGrounding() {
		auto* transform = GetTransform();
		if (!transform || !mPhysics) { return; }

		UPhysics::Hit groundHit = {};
		const bool    hitGround = mPhysics->BoxCast(
			BuildPhysicsBoxAt(transform->Position()),
			Vec3::down,
			Math::HtoM(mGroundSnapHu),
			&groundHit
		);

		if (hitGround && groundHit.normal.y >= mGroundNormalY && mVelocity.y <=
		    0.0f) {
			transform->SetPosition(
				transform->Position() + Vec3::down * groundHit.t
			);
			mVelocity.y = 0.0f;
			mGrounded   = true;
		} else { mGrounded = false; }
	}

	void MovementComponent::TryBlink(const InputFrame& input) {
		if (!input.wishBlink || mBlinkCooldownRemaining > 0.0f) { return; }

		auto* transform = GetTransform();
		if (!transform) { return; }

		const float yawDegrees = mCameraRotator ?
			                         mCameraRotator->GetLookAnglesDegrees().y :
			                         0.0f;
		const Vec3 direction = Mat4::RotateY(yawDegrees * Math::deg2Rad).
		                       GetForward().Normalized();
		Vec3 targetPos = transform->Position() + direction * Math::HtoM(
			                 mBlinkDistanceHu
		                 );
		if (mPhysics) {
			UPhysics::Hit hit = {};
			if (mPhysics->BoxCast(
				BuildPhysicsBoxAt(transform->Position()),
				direction,
				Math::HtoM(mBlinkDistanceHu),
				&hit
			)) {
				targetPos = transform->Position() + direction * std::max(
					            0.0f, hit.t - Math::HtoM(2.0f)
				            );
			}
		}

		transform->SetPosition(targetPos);
		mGrounded               = false;
		mBlinkCooldownRemaining = mBlinkCooldownSec;
	}

	void MovementComponent::TryStartWallrun() {
		if (!mPhysics || mWallRunning || mGrounded) { return; }

		Vec3 horizontal = mVelocity;
		horizontal.y    = 0.0f;
		if (Math::MtoH(horizontal.Length()) < mWallrunMinSpeedHu) { return; }

		const float checkDistance = Math::HtoM(mWidthHu * 0.5f + 12.0f);
		const std::array<Vec3, 2> directions = {Vec3::right, Vec3::left};
		for (const Vec3& side : directions) {
			UPhysics::Hit hit = {};
			if (!mPhysics->BoxCast(
				BuildPhysicsBoxAt(GetFeetPosition()),
				side,
				checkDistance,
				&hit
			)) { continue; }
			if (std::abs(hit.normal.y) > 0.2f) { continue; }

			mWallRunning   = true;
			mWallNormal    = hit.normal.Normalized();
			mWallDirection = Math::ProjectOnPlane(
				horizontal.Normalized(),
				mWallNormal
			);
			if (mWallDirection.SqrLength() <= 1e-6f) {
				mWallDirection = Vec3::up.Cross(mWallNormal).Normalized();
			}
			mWallrunTime = 0.0f;
			break;
		}
	}

	void MovementComponent::UpdateWallrun(
		const float deltaTime, const InputFrame& input
	) {
		if (!mWallRunning) { return; }

		mWallrunTime += deltaTime;
		if (input.wishCrouch || mWallrunTime > 2.5f) {
			EndWallrun();
			return;
		}

		if (input.wishJump && !mWasJumpHeldLastFrame) {
			mVelocity = mWallDirection * Math::HtoM(mWallrunMinSpeedHu) +
			            (mWallNormal * 0.7f + Vec3::up).Normalized() *
			            Math::HtoM(mWallrunJumpHu);
			EndWallrun();
			return;
		}

		if (!mPhysics) { return; }
		UPhysics::Hit hit = {};
		if (!mPhysics->BoxCast(
			BuildPhysicsBoxAt(GetFeetPosition()),
			-mWallNormal,
			Math::HtoM(mWidthHu * 0.5f + 12.0f),
			&hit
		)) {
			EndWallrun();
			return;
		}

		Vec3 horizontal = mWallDirection * std::max(
			                  Math::HtoM(mWallrunMinSpeedHu),
			                  Vec3(mVelocity.x, 0.0f, mVelocity.z).Length()
		                  );
		mVelocity.x = horizontal.x;
		mVelocity.z = horizontal.z;
	}

	void MovementComponent::TryStartSlide() {
		if (!mGrounded || mSliding) { return; }
		Vec3 horizontal = mVelocity;
		horizontal.y    = 0.0f;
		if (Math::MtoH(horizontal.Length()) < mSlideMinSpeedHu) { return; }

		mSliding        = true;
		mSlideTime      = 0.0f;
		mSlideDirection = horizontal.Normalized();
		mVelocity       += mSlideDirection * Math::HtoM(mSlideBoostHu);
	}

	void MovementComponent::UpdateSlide(
		const float deltaTime, const InputFrame& input
	) {
		if (!mSliding) { return; }
		mSlideTime += deltaTime;
		if (!input.wishCrouch) {
			EndSlide();
			return;
		}

		Vec3 horizontal     = mVelocity;
		horizontal.y        = 0.0f;
		const float speedHu = Math::MtoH(horizontal.Length());
		if (speedHu <= mSlideStopHu || mSlideTime > 20.0f) {
			EndSlide();
			return;
		}

		mVelocity.x *= 0.995f;
		mVelocity.z *= 0.995f;
	}

	void MovementComponent::EndSlide() {
		mSliding        = false;
		mSlideTime      = 0.0f;
		mSlideDirection = Vec3::zero;
	}

	void MovementComponent::EndWallrun() {
		mWallRunning   = false;
		mWallNormal    = Vec3::zero;
		mWallDirection = Vec3::zero;
		mWallrunTime   = 0.0f;
	}

	REGISTER_COMPONENT(MovementComponent);
}
