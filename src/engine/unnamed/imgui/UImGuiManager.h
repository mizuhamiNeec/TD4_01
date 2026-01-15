#pragma once
#include <d3d12.h>

namespace Unnamed {
	class DescriptorAllocator;
	class GraphicsDevice;

	/// @brief ImGuiマネージャークラス
	/// @details 新エンジン用のImGuiマネージャークラス
	class UImGuiManager {
	public:
		/// @brief コンストラクタ
		/// @param graphicsDevice グラフィックスデバイス
		/// @param hWnd ウィンドウハンドル
		explicit UImGuiManager(GraphicsDevice* graphicsDevice, void* hWnd);

		/// @brief フレーム開始
		void BeginFrame();

		/// @brief フレーム終了
		void EndFrame(ID3D12GraphicsCommandList* cmd);

		/// @brief シャットダウン
		void Shutdown();

	private:
		/// @brief ImGuiのアロケータ・フリー関数用
		[[nodiscard]] DescriptorAllocator* GetSrvAllocator();

	private:
		GraphicsDevice* mGraphicsDevice = nullptr;
	};
}
