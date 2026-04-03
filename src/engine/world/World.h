#pragma once
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "core/guidgenerator/GuidGenerator.h"
#include "core/math/Vec4.h"

#include "engine/world/GameplayCueBus.h"
#include "engine/world/WorldCameraManager.h"
#include "engine/world/WorldDebugDraw.h"

namespace Unnamed {
	namespace Physics {
		class Engine;
	}

	class Scene;
	class AssetManager;
	class JsonReader;

	namespace Render {
		struct RenderFrameContext;
		struct RenderFrameInputs;
		struct RenderCameraInput;
	}

	struct WorldTime {
		float    fixedDeltaTime        = 0.0f;
		float    renderDeltaTime       = 0.0f;
		float    renderUnscaledDeltaTime = 0.0f;
		float    interpolationAlpha    = 0.0f;
		float    timeSeconds           = 0.0f;
		uint64_t fixedTickCounter      = 0;
	};

	class World {
	public:
		virtual ~World();

		/// @brief シーンを取得します。存在しない場合はnullptrを返します。
		[[nodiscard]] Scene& GetScene();

		/// @brief シーンを取得します。存在しない場合はnullptrを返します。
		[[nodiscard]] const Scene& GetScene() const;

		[[nodiscard]] Scene* GetScenePtr() noexcept;

		[[nodiscard]] const Scene* GetScenePtr() const noexcept;

		/// @brief 初期化します。
		virtual void Initialize();

		/// @brief シャットダウンします。
		virtual void Shutdown();

		/// @brief 互換目的のティック。内部的には FixedTick と RenderTick を順番に実行します。
		virtual void Tick(float unscaledDeltaTime, float deltaTime);

		/// @brief 固定シミュレーションティックを実行します。
		virtual void FixedTick(float fixedDeltaTime);

		/// @brief 固定シミュレーション前に、フレーム入力を反映します。
		virtual void FrameInputTick(float frameDeltaTime);

		/// @brief 描画フレームティックを実行します。
		virtual void RenderTick(float renderDeltaTime, float interpolationAlpha);

		/// @brief エディタ用のティックします。ゲームの時間スケールの影響を受けません。
		/// @param unscaledDeltaTime 前のフレームからの経過時間（秒）。ゲームの時間スケールの影響を受けません。
		virtual void EditorTick(float unscaledDeltaTime);

		/// @brief ファイルからシーンをロードします。
		/// @param path ロードするシーンのファイルパス
		/// @return ロードに成功した場合はtrue、失敗した場合はfalse
		virtual bool LoadSceneFromFile(const char* path);

		/// @brief シーンをファイルに保存します。
		/// @param path 保存するシーンのファイルパス
		/// @return 保存に成功した場合はtrue、失敗した場合はfalse
		virtual bool SaveSceneToFile(const char* path) const;

		/// @brief 現在のシーンをアンロードします。
		virtual void UnloadScene();

		/// @brief レンダリングフレームの入力を埋めます。
		virtual void FillRenderFrameInputs(
			Render::RenderFrameInputs&  inputs,
			Render::RenderFrameContext& frameContext,
			AssetManager&               assetManager
		);

		/// @brief ゲームシミュレーションが有効かどうかを返します。デフォルトではtrueを返します。
		/// @return ゲームシミュレーションが有効な場合はtrue、そうでない場合はfalse
		[[nodiscard]] virtual bool IsGameSimulationEnabled() const noexcept;

		/// @brief ロードされたシーンのファイルパスを取得します。存在しない場合は空文字列を返します。
		/// @return ロードされたシーンのファイルパス
		[[nodiscard]] std::string_view GetLoadedScenePath() const;

		/// @brief ロードされたシーンのファイルパスを設定します。
		/// @param path ロードされたシーンのファイルパス
		void SetLoadedScenePath(std::string path);

		/// @brief ワールドの現在カメラマネージャを取得します。
		/// @return ワールドの現在カメラマネージャへの参照
		[[nodiscard]] WorldCameraManager& GetCameraManager() noexcept;

		/// @brief ワールドの現在カメラマネージャを取得します（const版）。
		/// @return ワールドの現在カメラマネージャへのconst参照
		[[nodiscard]] const WorldCameraManager&
		GetCameraManager() const noexcept;

		[[nodiscard]] GameplayCueBus& GetGameplayCueBus() noexcept;
		[[nodiscard]] const GameplayCueBus& GetGameplayCueBus() const noexcept;

		/// @brief シーンを設定します。既にシーンが存在する場合はアンロードされます。
		/// @param scene 設定するシーンのユニークポインタ
		virtual void SetScene(std::unique_ptr<Scene> scene);

		[[nodiscard]] Physics::Engine& GetPhysicsEngine();

		[[nodiscard]] const Physics::Engine& GetPhysicsEngine() const;

		/// @brief 指定したポストFXパスの有効/無効を上書きします。
		/// @param passName ポストFXパス名（例: "Vignette"）
		/// @param enabled trueなら有効、falseなら無効
		void SetPostFxPassEnabledOverride(
			std::string_view passName, bool enabled
		);

		/// @brief 指定したポストFXパスの有効/無効上書きを解除します。
		/// @param passName ポストFXパス名
		void ClearPostFxPassEnabledOverride(std::string_view passName);

		/// @brief すべてのポストFXパス有効/無効上書きを解除します。
		void ClearPostFxPassEnabledOverrides();

		/// @brief 指定したポストFXパスのスカラー値を上書きします。
		/// @param passName ポストFXパス名
		/// @param paramName パラメータ名（例: "Intensity"）
		/// @param value スカラー値
		void SetPostFxScalarOverride(
			std::string_view passName, std::string_view paramName, float value
		);

		/// @brief 指定したポストFXパスの色値を上書きします。
		/// @param passName ポストFXパス名
		/// @param paramName パラメータ名（例: "Tint"）
		/// @param value 色値
		void SetPostFxColorOverride(
			std::string_view passName, std::string_view paramName,
			const Vec4&      value
		);

		/// @brief 指定したポストFXパスのパラメータ上書きを解除します。
		void ClearPostFxParamOverride(
			std::string_view passName, std::string_view paramName
		);

		/// @brief 指定したポストFXパスの全上書きを解除します。
		void ClearPostFxPassOverrides(std::string_view passName);

		/// @brief すべてのポストFXパス上書きを解除します。
		void ClearAllPostFxOverrides();

		/// @brief ワールド共通のデバッグ描画コンテキストを取得します。
		[[nodiscard]] WorldDebugDraw& GetDebugDraw() noexcept;

		/// @brief ワールド共通のデバッグ描画コンテキストを取得します（const版）。
		[[nodiscard]] const WorldDebugDraw& GetDebugDraw() const noexcept;

		[[nodiscard]] const WorldTime& GetTime() const noexcept;

	protected:
		struct PostFxPassOverrides {
			std::unordered_map<std::string, Vec4>  colorParams;
			std::unordered_map<std::string, float> scalarParams;
			std::optional<bool>                    enabled;
		};

		[[nodiscard]] virtual bool BuildUiInputCamera(
			Render::RenderCameraInput& outCamera
		) const;

		/// @brief シーンがロードされたときに呼び出されます。派生クラスでオーバーライドして、シーンロード時の処理を実装できます。
		virtual void OnSceneLoaded();

		/// @brief シーンがアンロードされる前に呼び出されます。派生クラスでオーバーライドして、シーンアンロード時の処理を実装できます。
		virtual void OnSceneUnloaded();

		std::unordered_map<std::string, PostFxPassOverrides>
		mPostFxPassOverrides;

		std::unique_ptr<Scene>           mScene;           // 現在のシーン
		std::unique_ptr<Physics::Engine> mPhysicsEngine;   // 物理エンジン
		WorldCameraManager               mCameraManager;   // ワールドの現在カメラ管理
		GameplayCueBus                   mGameplayCueBus;
		GuidGenerator                    mGuidGenerator;   // GUIDジェネレーター
		std::string                      mLoadedScenePath; // ロードされたシーンのファイルパス
		WorldTime                        mTime;            // ワールドの時間情報
		WorldDebugDraw                   mDebugDraw;
	};
}
