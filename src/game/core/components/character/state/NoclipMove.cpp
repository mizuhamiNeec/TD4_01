#include "NoclipMove.h"

namespace Unnamed {
	void NoclipMove::Enter(ConsoleSystem* console) {
		console;
	}

	void NoclipMove::Tick(MovementContext& context, float deltaTime) {
		context.isGrounded = false; // Noclip中は常に空中にいるとみなす

		deltaTime;
		
		// Vec3  wishDir  = GetWishDir(context); // Noclipは水平・垂直両方の入力を考慮する
		// float maxSpeed = mMaxSpeed->GetValue() * mNoclipSpeed->GetValue();
		// float wishSpeed = 
		//
		// Accelerate(
		// 	context.velocity, wishDir, maxSpeed,
		// 	mNoclipAccelerate->GetValue(),
		// 	deltaTime
		// );
	}

	void NoclipMove::Exit() {}

	std::string_view NoclipMove::GetStateName() {
		return "NoclipMove";
	}
}
