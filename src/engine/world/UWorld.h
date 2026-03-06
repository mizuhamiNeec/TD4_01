#pragma once
#include <memory>
#include <string>
#include <string_view>

#include <engine/scene/UScene.h>

#include "core/guidgenerator/GuidGenerator.h"

namespace Unnamed {
	class AssetManager;
	class JsonReader;

	namespace Render {
		struct RenderFrameContext;
		struct RenderFrameInputs;
	}

	struct WorldTime {
		float deltaTime         = 0.0f;
		float unscaledDeltaTime = 0.0f;
		float timeSeconds       = 0.0f;
	};

	class UWorld {
	public:
		virtual ~UWorld();

		/// @brief 現在ティック中のワールドを取得します。存在しない場合はnullptrを返します。
		[[nodiscard]] static UWorld* GetTickingWorld() noexcept;

		/// @brief シーンを取得します。存在しない場合はnullptrを返します。
		[[nodiscard]] UScene& GetScene() {
			return *mScene;
		}

		/// @brief シーンを取得します。存在しない場合はnullptrを返します。
		[[nodiscard]] const UScene& GetScene() const {
			return *mScene;
		}

		/// @brief 初期化します。
		virtual void Initialize();
		/// @brief シャットダウンします。
		virtual void Shutdown();
		/// @brief ティックします。
		/// @param deltaTime 前のフレームからの経過時間（秒）
		virtual void Tick(float deltaTime);

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

		/// @brief シーンを設定します。既にシーンが存在する場合はアンロードされます。
		/// @param scene 設定するシーンのユニークポインタ
		virtual void SetScene(std::unique_ptr<UScene> scene);

	protected:
		/// @brief シーンがロードされたときに呼び出されます。派生クラスでオーバーライドして、シーンロード時の処理を実装できます。
		virtual void OnSceneLoaded();

		/// @brief シーンがアンロードされる前に呼び出されます。派生クラスでオーバーライドして、シーンアンロード時の処理を実装できます。
		virtual void OnSceneUnloaded();

		std::unique_ptr<UScene> mScene;           // 現在のシーン
		GuidGenerator           mGuidGenerator;   // GUIDジェネレーター
		std::string             mLoadedScenePath; // ロードされたシーンのファイルパス
		WorldTime               mTime;            // ワールドの時間情報
	};
}
