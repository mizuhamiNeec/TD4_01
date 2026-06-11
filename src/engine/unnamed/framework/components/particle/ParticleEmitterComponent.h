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

		// -----------------------------------------------------------------------
		// 実行時制御（ゲームプレイ側から駆動するためのAPI）
		// -----------------------------------------------------------------------

		/// @brief 自動発生（ループ）を開始します。
		void Play();

		/// @brief 自動発生を停止します（既存粒子はそのまま寿命まで残ります）。
		void Stop();

		/// @brief いま即座に1バースト（count個）発生させます。autoPlay/repeatに依存せず単発で使えます。
		void FireBurst();

		/// @brief リングなどの最終的な広がり半径をワールド単位で指定します（scaleSpeed を逆算）。
		void SetRadius(float worldRadius);

		/// @brief 色の明るさ/濃さを 0〜1 の強度で指定します（colorCurve の定数倍率として適用）。
		void SetIntensity(float intensity01);

		/// @brief 発生数を、ロード時の基準 count に対する倍率で指定します（最低1個）。
		void SetCountScale(float scale);

		/// @brief 自動発生中かどうかを取得します。
		[[nodiscard]] bool IsPlaying() const;

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

		/// @brief 発生形状(Box/Sphere)をデバッグ描画します。
		void DrawEmitterShapeDebug() const;

		std::string mPresetName;
		std::string mPresetPath;
		bool        mAutoPlay         = true;
		bool        mDepthTest        = true;
		bool        mDrawEmitterShape = true; // 発生形状のデバッグ描画
		int32_t     mSortKeyBias      = 0;
		float       mTimeScale        = 1.0f;

		ParticlePresetLibrary   mPresetLibrary;
		ParticleEmitterInstance mEmitter;
		const ParticlePreset*   mPreset = nullptr;
		ParticlePreset*         mMutablePreset = nullptr; // mPresetLibrary 内の可変実体（実行時の値上書き用）
		uint32_t                mBaseSpawnCount = 0;       // ロード時点の基準 count（SetCountScale の基準）
		bool                    mHasWarnedLoadFailure = false;

		std::string mCachedTexturePath;
		AssetID     mCachedTextureAssetId = kInvalidAssetID;
		std::string mLoadedPresetPath;
		AssetID     mPresetAssetId      = kInvalidAssetID;
		uint32_t    mPresetAssetVersion = 0;
	};
}
