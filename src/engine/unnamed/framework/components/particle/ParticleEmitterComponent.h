#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "engine/particle/Particle/ParticleEmitterInstance.h"
#include "engine/particle/ParticlePresetLibrary.h"
#include "engine/render/frame/RenderFrameInputs.h"

#include "../base/BaseComponent.h"

namespace Unnamed {
	/// @brief パーティクルプリセットを再生し、描画入力へ変換するコンポーネントです。
	class ParticleEmitterComponent final : public BaseComponent {
	public:
		/// @brief コンポーネントの安定名を取得します。
		[[nodiscard]] std::string_view GetStableName() const override;

		/// @brief コンポーネント表示名を取得します。
		[[nodiscard]] std::string_view GetComponentName() const override;

		/// @brief 描画フレームに先行してパーティクル更新を行います。
		void OnTick(float deltaTime) override;

#ifdef _DEBUG
		/// @brief Inspectorで編集UIを描画します。
		void DrawInspectorImGui() override;
#endif

		/// @brief JSONから値を読み込みます。
		void Deserialize(const JsonReader& reader) override;

		/// @brief JSONへ値を書き込みます。
		void Serialize(JsonWriter& writer) const override;

		/// @brief 現在の粒子をRender入力へ変換して追加します。
		void GatherWorldParticles(std::vector<Render::WorldParticleInput>& outParticles);

	private:
		[[nodiscard]] bool EnsurePresetLoaded();
		[[nodiscard]] std::string ResolvePresetFilePath() const;
		[[nodiscard]] bool EnsurePresetAssetTracked(
			const std::string& resolvedPresetPath
		);
		[[nodiscard]] bool LoadPresetFromFile(const std::string& resolvedPresetPath);
		void               ResetEmitterFromPreset();
		[[nodiscard]] AssetID ResolveTextureAssetId();
		[[nodiscard]] Render::WORLD_PARTICLE_SHAPE ResolveRenderShape() const;
		[[nodiscard]] Render::WORLD_PARTICLE_BLEND_MODE ResolveRenderBlendMode() const;

		std::string mPresetName;
		std::string mPresetPath;
		bool        mAutoPlay    = true;
		bool        mDepthTest   = true;
		int32_t     mSortKeyBias = 0;
		float       mTimeScale   = 1.0f;

		ParticlePresetLibrary   mPresetLibrary;
		ParticleEmitterInstance mEmitter;
		const ParticlePreset*   mPreset = nullptr;
		bool                    mHasWarnedLoadFailure = false;

		std::string mCachedTexturePath;
		AssetID     mCachedTextureAssetId = kInvalidAssetID;
		std::string mLoadedPresetPath;
		AssetID     mPresetAssetId      = kInvalidAssetID;
		uint32_t    mPresetAssetVersion = 0;
	};
}
