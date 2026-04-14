#pragma once

#include <cstdint>

#include "engine/render/frame/RenderFrameInputs.h"

namespace Unnamed {
	class World;

	class WorldCameraManager final {
	public:
		struct CurrentCameraInfo {
			uint64_t                  entityGuid = 0;
			Render::RenderCameraInput camera     = {};
			bool                      valid      = false;
		};

		explicit WorldCameraManager(World* world = nullptr);

		void SetWorld(World* world) noexcept;

		bool SetCurrentCamera(uint64_t entityGuid);
		void ClearCurrentCamera();

		[[nodiscard]] uint64_t GetCurrentCameraGuid() const noexcept;
		[[nodiscard]] bool     HasCurrentCamera() const noexcept;

		bool ResolveForRender(
			float aspect, Render::RenderCameraInput& outCamera
		);
		[[nodiscard]] CurrentCameraInfo GetCurrentCameraInfo() const noexcept;

	private:
		void ResetResolvedInfo() noexcept;

		World*            mWorld             = nullptr;
		uint64_t          mCurrentCameraGuid = 0;
		CurrentCameraInfo mLastResolvedInfo  = {};
	};
}
