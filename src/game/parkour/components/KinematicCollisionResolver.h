#pragma once

#include "engine/physics/core/UPhysics.h"

namespace Unnamed {
	// class UBaseComponent;
	// class TransformComponent;
	// class UPhysics::Engine;
	//
	// class KinematicCollisionResolver {
	// public:
	// 	KinematicCollisionResolver(
	// 		UPhysics::Engine*   physicsEngine, MovementData* data,
	// 		TransformComponent* transform, Box*              hull
	// 	);
	//
	// 	void MoveWithCollisions(float dt);
	//
	// 	bool CollideAndSlide(
	// 		const Vec3& startPos,
	// 		const Vec3& displacement,
	// 		Vec3&       outPos
	// 	);
	//
	// 	void SyncHullFromComponent();
	//
	// 	void DetectAndResolveStuck(float dt);
	//
	// 	[[nodiscard]] Box BuildHullAtFeet(const Vec3& feetPos) const;
	//
	// 	void              SetData(MovementData* movementData);
	//
	// private:
	// 	[[nodiscard]] float StepHeightM() const;
	// 	[[nodiscard]] float CastSkinM() const;
	// 	[[nodiscard]] float SkinM() const;
	// 	[[nodiscard]] float RestOffsetM() const;
	// 	[[nodiscard]] float MaxAdhesionM() const;
	//
	// 	void ResolvePenetration();
	// 	int  SlideMove(Vec3& position, Vec3& velocity, float timeTotal);
	// 	void StepMove(Vec3& position, Vec3& velocity, float timeTotal);
	// 	bool GroundCheck(Vec3& position);
	//
	// 	Vec3 ClipVelocity(
	// 		const Vec3& vel, const Vec3& normal, float overbounce
	// 	);
	// 	void UpdateEntityHullDimensions();
	//
	// 	UPhysics::Engine*   mPhysicsEngine = nullptr;
	// 	MovementData*       mData          = nullptr;
	// 	TransformComponent* mTransform     = nullptr;
	// 	Box*                mHull          = nullptr;
	// };
}
