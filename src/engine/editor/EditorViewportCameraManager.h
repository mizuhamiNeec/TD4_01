#pragma once
#ifdef _DEBUG

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "core/math/Vec3.h"

#include "engine/render/frame/RenderFrameInputs.h"

namespace Unnamed {
	class EditorWorld;

	enum class ViewportCameraBindingKind : uint8_t {
		EditorPerspective = 0,
		EditorOrthoTop    = 1,
		EditorOrthoFront  = 2,
		EditorOrthoRight  = 3,
		ActiveGameCamera  = 4,
		CameraEntity      = 5,
	};

	struct ViewportCameraBinding {
		ViewportCameraBindingKind kind = ViewportCameraBindingKind::
			EditorPerspective;
		uint64_t cameraEntityGuid = 0;
	};

	struct ViewportOrthoState {
		Vec3  center = Vec3::zero;
		float zoom   = 1024.0f;
	};

	class EditorViewportCameraManager final {
	public:
		struct ResolvedCamera {
			Render::RenderCameraInput input = {};
			bool                      isOrthographic = false;
		};

		void SetPaneBinding(
			std::string_view             viewKey,
			const ViewportCameraBinding& binding
		);
		[[nodiscard]] ViewportCameraBinding GetPaneBinding(
			std::string_view viewKey
		) const;

		void SetOrthoState(
			std::string_view          viewKey,
			const ViewportOrthoState& state
		);
		[[nodiscard]] ViewportOrthoState GetOrthoState(
			std::string_view viewKey
		) const;

		ResolvedCamera ResolveViewCamera(
			EditorWorld&                     editorWorld,
			std::string_view                 viewKey,
			const Render::SceneViewRenderMode& sceneViewMode,
			const ViewportCameraBinding&     binding,
			const Render::RenderCameraInput* fallbackCamera
		);

		void SyncGameplayCameraAspect(
			EditorWorld&                     editorWorld,
			const Render::SceneViewRenderMode& sceneViewMode,
			const ViewportCameraBinding&     binding
		);

	private:
		std::unordered_map<std::string, ViewportCameraBinding> mPaneBindings;
		std::unordered_map<std::string, ViewportOrthoState> mPaneOrthoStates;
	};
}

#endif
