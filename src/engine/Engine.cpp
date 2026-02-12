#include "Engine.h"

#include <pch.h>

#include "core/assets/AssetManager.h"
#include "core/assets/loader/TextureLoaderDirectXTex.h"

#include "engine/ImGui/ImGuiManager.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "engine/Line/LineCommon.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "engine/Model/ModelCommon.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "engine/Object3D/Object3DCommon.h"
#include "engine/particle/ParticleManager.h"
#include "engine/renderer/ConstantBuffer.h"
#include "engine/renderer/IndexBuffer.h"
#include "engine/ResourceSystem/Audio/AudioManager.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "engine/Sprite/SpriteCommon.h"
#include "engine/TextureManager/TexManager.h"

#include "Platform/PlatformEventsImpl.h"
#include "Platform/WindowManager.h"

#include "render/rendergraph/RenderGraph.h"

#include "ResourceSystem/Manager/ResourceManager.h"

#include "rhi/d3d12/D3D12CommandContext.h"
#include "rhi/d3d12/D3D12Device.h"
#include "rhi/interface/IRhiDevice.h"

#include "unnamed/subsystem/console/concommand/UnnamedConCommand.h"
#include "unnamed/subsystem/input/device/keyboard/KeyboardDevice.h"
#include "unnamed/subsystem/input/device/mouse/MouseDevice.h"
#include "unnamed/subsystem/terminal/TerminalSystem.h"
#include "unnamed/subsystem/time/SystemClock.h"
#include "unnamed/subsystem/time/TimeSystem.h"

#include "world/UWorld.h"

#ifdef _DEBUG
#include <imgui_internal.h>
// ImGuizmoのインクルードはImGuiより後! いいね!?
#endif

namespace Unnamed {
	namespace Rhi {
		class D3D12CommandContext;
		class D3D12Device;
	}

	/// @brief コンストラクタ
	Engine::Engine() = default;

	/// @brief デストラクタ
	Engine::~Engine() = default;

	int Engine::Run() {
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); // リークチェック
		[[maybe_unused]] HRESULT hr = CoInitializeEx(
			nullptr, COINIT_MULTITHREADED
		);
		timeBeginPeriod(1); // システムタイマーの分解能を上げる

		// 初期化
		if (!Init()) { UASSERT(false && "Failed to initialize Engine"); }

		// メインループ
		while (true) {
			mWindowManager->ProcessMessage();

			// ウィンドウのリサイズ処理
			for (const WindowId id : mWindowManager->GetAllWindowIds()) {
				Window* wnd = mWindowManager->FindWindowById(id);
				if (!wnd) { continue; }
				if (const auto resize = wnd->ConsumeResizeEvent()) {
					mRhiDevice->OnResize(resize->width, resize->height);
					//OnResize(resize->width, resize->height);
				}
			}

			if (mWindowManager->ShouldQuit() || mWishShutdown) { break; }

			Tick();
		}

		// シャットダウン
		Shutdown();
		timeEndPeriod(1);
		CoUninitialize();
		return EXIT_SUCCESS;
	}

	D3D12* Engine::GetRendererInstance() const { return mRenderer.get(); }

	ResourceManager* Engine::GetResourceManagerInstance() const {
		return mResourceManager.get();
	}

	SpriteCommon* Engine::GetSpriteCommonInstance() const {
		return mSpriteCommon.get();
	}

	ParticleManager* Engine::GetParticleManagerInstance() const {
		return mParticleManager.get();
	}

	SrvManager* Engine::GetSrvManagerInstance() const {
		return mResourceManager->GetSrvManager();
	}

	TexManager* Engine::GetTexManagerInstance() const {
		return mResourceManager ?
			       mResourceManager->GetTexManager() :
			       nullptr;
	}

	Vec2   Engine::GetViewportLTInstance() const { return mViewportLT; }
	Vec2   Engine::GetViewportSizeInstance() const { return mViewportSize; }
	float& Engine::GetBlurStrengthInstance() { return mBlurStrength; }

	std::size_t Engine::GetPostChainSize() const {
		return mPostProcessPipeline.GetPassCount();
	}

	IPostProcess* Engine::GetPostProcessAt(const int index) const {
		if (index < 0) { return nullptr; }
		return mPostProcessPipeline.GetPassAt(static_cast<size_t>(index));
	}

	uint64_t Engine::GetActivePingSrvGpuPtr() const {
		return mPostProcessPipeline.GetActivePingSrvGpuPtr();
	}

	D3D12_RESOURCE_DESC Engine::GetActivePingRtvDesc() const {
		return mPostProcessPipeline.GetActivePingRtvDesc();
	}

	template <class TWorld, class... Args>
	TWorld& Engine::SwitchWorld(Args&&... args) {
		static_assert(std::is_base_of_v<UWorld, TWorld>);

		if (mWorld) {
			mWorld->Shutdown();
			mWorld.reset();
		}

		auto newWorld = std::make_unique<TWorld>(
			std::forward<Args>(args)...
		);
		TWorld* raw = newWorld.get();

		mWorld = std::move(newWorld);

		mWorld->Initialize();
		return *raw;
	}

	UWorld* Engine::GetWorld() const { return mWorld.get(); }

	AudioManager* Engine::GetAudioManagerInstance() const {
		return mAudioManager.get();
	}

	/// @brief 初期化
	/// @return 成功したらtrueを返す
	bool Engine::Init() {
		SystemClock::Init();

		ServiceLocator::Register<Engine>(this);

		mConfig = {
#ifdef _DEBUG
			.mode = RUN_MODE::EDITOR,
#else
			.mode = RUN_MODE::STANDALONE,
#endif
			.window = {
				.title     = "Unnamed Engine",
				.width     = 1280,
				.height    = 720,
				.mode      = WINDOW_MODE::WINDOWED,
				.resizable = true
			},
		};

		// WindowManagerの初期化メインウィンドウ作成
		mWindowManager = std::make_unique<WindowManager>();
		if (!mWindowManager->Init(mConfig.window)) { return false; }

		// メインウィンドウのID取得
		const auto id = mWindowManager->GetMainWindowId();
		// メインウィンドウのポインタ取得
		const auto window = mWindowManager->FindWindowById(id);
		// HWND取得
		auto hwnd = window->GetHwnd();

		// ConsoleSystemの初期化
		mConsoleSystem = std::make_unique<ConsoleSystem>();
		if (!mConsoleSystem->Init()) { return false; }

		// TerminalSystem の初期化（ConsoleSystem 経由で操作するため Console を渡す）
		mTerminalSystem = std::make_unique<
			TerminalSystem>(mConsoleSystem.get());
		if (!mTerminalSystem->Init()) { return false; }

		mAssetManager = std::make_unique<AssetManager>();

		// 各ローダーの登録
		mAssetManager->RegisterLoader(
			std::move(std::make_unique<TextureLoaderDirectXTex>())
		);

		// TimeSystemの初期化
		mTimeSystem = std::make_unique<TimeSystem>();
		if (!mTimeSystem->Init()) { return false; }

		// InputSystemの初期化
		mInputSystem = std::make_unique<UInputSystem>();
		if (!mInputSystem->Init()) { return false; }
		// デバイス登録
		const auto keyboardDevice = std::make_shared<KeyboardDevice>(hwnd);
		const auto mouseDevice    = std::make_shared<MouseDevice>(hwnd);
		mInputSystem->RegisterDevice(keyboardDevice);
		mInputSystem->RegisterDevice(mouseDevice);

		// プラットフォームイベントの作成
		mPlatformEvents = std::make_unique<PlatformEventsImpl>();
		// ウィンドウマネージャに登録
		mWindowManager->RegisterPlatformEvents(mPlatformEvents.get());
		// 入力システムをリスナーに登録
		mPlatformEvents->AddListener(mInputSystem.get());

		constexpr Rhi::DeviceDesc deviceDesc = {
			.enableDebugLayer         = true,
			.enableGpuBasedValidation = true
		};

		const Rhi::SwapChainDesc swapChainDesc = {
			.width       = static_cast<uint32_t>(window->GetDesc().width),
			.height      = static_cast<uint32_t>(window->GetDesc().height),
			.bufferCount = 2,
			.vSync       = false
		};

		mRhiDevice = Rhi::CreateD3D12Device(
			hwnd,
			deviceDesc,
			swapChainDesc
		);

		mGraph = std::make_unique<Render::RenderGraph>();

		mGraph->AddPass(
			"ClearPass",
			[](Render::RenderGraphBuilder& builder) {
				builder.WriteBackBufferRt();
			},
			[](Rhi::IRhiCommandContext& ctx) {
				Rhi::ClearColor color = {};
				color.r               = 0.1f;
				color.g               = 0.1f;
				color.b               = 0.1f;
				color.a               = 1.0f;
				ctx.ClearBackBuffer(color);
			}
		);

		mGraph->AddPass(
			"ComputeWriteUav",
			[](Render::RenderGraphBuilder& builder) { builder.WriteUav(1); },
			[this](Rhi::IRhiCommandContext& ctx) {
				// DX12
				auto& dxDevice = static_cast<Rhi::D3D12Device&>(*mRhiDevice);
				auto& dxCtx    = static_cast<Rhi::D3D12CommandContext&>(ctx);

				dxCtx.SetSrvUavHeap(dxDevice.GetSrvUavHeap());
				dxCtx.SetComputePipeline(
					dxDevice.GetCsRootSignature(),
					dxDevice.GetCsPipelineStateObject()
				);
				dxCtx.SetComputeRootUavTable(
					0, dxDevice.GetIntermediateUavGPU()
				);

				// dispatchサイズ
				const uint32_t w  = dxDevice.GetSwapChain().GetWidth();
				const uint32_t h  = dxDevice.GetSwapChain().GetHeight();
				const uint32_t gx = (w + 7) / 8;
				const uint32_t gy = (h + 7) / 8;

				dxCtx.Dispatch(gx, gy, 1);
			}
		);

		mGraph->AddPass(
			"FullscreenSampleSrv",
			[](Render::RenderGraphBuilder& builder) {
				builder.ReadSrv(1);
				builder.WriteBackBufferRt();
			},
			[this](Rhi::IRhiCommandContext& ctx) {
				auto& dxDevice = static_cast<Rhi::D3D12Device&>(*mRhiDevice);
				auto& dxCtx    = static_cast<Rhi::D3D12CommandContext&>(ctx);

				// ビューポート、シザーの設定
				{
					const float width =
						static_cast<float>(dxDevice.GetSwapChain().GetWidth());
					const float height =
						static_cast<float>(dxDevice.GetSwapChain().GetHeight());

					D3D12_VIEWPORT viewport;
					viewport.TopLeftX = 0.0f;
					viewport.TopLeftY = 0.0f;
					viewport.Width    = width;
					viewport.Height   = height;
					viewport.MinDepth = 0.0f;
					viewport.MaxDepth = 1.0f;

					D3D12_RECT rect;
					rect.left   = 0;
					rect.top    = 0;
					rect.right  = static_cast<LONG>(width);
					rect.bottom = static_cast<LONG>(height);

					dxDevice.GetCommandList()->RSSetViewports(1, &viewport);
					dxDevice.GetCommandList()->RSSetScissorRects(1, &rect);
				}

				dxCtx.SetSrvUavHeap(dxDevice.GetSrvUavHeap());
				dxCtx.SetGraphicsPipeline(
					dxDevice.GetFsRootSignature(),
					dxDevice.GetFsPipelineStateObject()
				);
				dxCtx.SetGraphicsRootSrvTable(
					0, dxDevice.GetIntermediateSrvGPU()
				);

				dxCtx.DrawFullScreenTriangle();
			}
		);

		// 		mRenderer = std::make_unique<D3D12>(hwnd, window->GetDesc());
		//
		// 		InputSystem::Init();
		//
		// 		RegisterConsoleCommandsAndVariables();
		//
		// 		// 各マネージャーの初期化
		// 		mResourceManager = std::make_unique<ResourceManager>(mRenderer.get());
		//
		// 		mResourceManager->Init();
		// 		mRenderer->SetShaderResourceViewManager(
		// 			mResourceManager->GetSrvManager()
		// 		);
		// 		mRenderer->Init();
		//
		// 		mAudioManager = std::make_unique<AudioManager>();
		// 		mAudioManager->Init();
		//
		// #ifdef _DEBUG
		// 		// ImGuiFontテクスチャ用にSRVを確保
		// 		mResourceManager->GetSrvManager()->AllocateForTexture2D();
		// 		// ImGuiManagerの初期化
		// 		mImGuiManager = std::make_unique<ImGuiManager>(
		// 			hwnd, mRenderer.get(), mResourceManager->GetSrvManager()
		// 		);
		// #endif
		//
		// 		// コンソールを作成
		// 		mConsole = std::make_unique<Console>();
		//
		// #pragma region PostProcessInit
		// 		mRenderTargets.Init(
		// 			mRenderer.get(),
		// 			window->GetDesc().width,
		// 			window->GetDesc().height,
		// 			kOffscreenClearColor,
		// 			kBufferFormat,
		// 			DXGI_FORMAT_D32_FLOAT
		// 		);
		//
		// 		mPostProcessPipeline.Init(
		// 			mRenderer.get(),
		// 			mResourceManager->GetSrvManager(),
		// 			window->GetDesc().width,
		// 			window->GetDesc().height,
		// 			reinterpret_cast<const float*>(&kOffscreenClearColor),
		// 			kBufferFormat,
		// 			DXGI_FORMAT_D32_FLOAT
		// 		);
		//
		// 		mPostProcessPipeline.AddPass(
		// 			std::make_unique<PPBloom>(
		// 				mRenderer->GetDevice(), mResourceManager->GetSrvManager()
		// 			)
		// 		);
		//
		// 		mPostProcessPipeline.AddPass(
		// 			std::make_unique<PPVignette>(
		// 				mRenderer->GetDevice(), mResourceManager->GetSrvManager()
		// 			)
		// 		);
		//
		// 		mPostProcessPipeline.AddPass(
		// 			std::make_unique<PPChromaticAberration>(
		// 				mRenderer->GetDevice(), mResourceManager->GetSrvManager()
		// 			)
		// 		);
		//
		// 		mPostProcessPipeline.AddPass(
		// 			std::make_unique<PPRadialBlur>(
		// 				mRenderer->GetDevice(), mResourceManager->GetSrvManager()
		// 			)
		// 		);
		// #pragma endregion
		//
		// 		// スプライト
		// 		mSpriteCommon = std::make_unique<SpriteCommon>();
		// 		mSpriteCommon->Init(mRenderer.get());
		//
		// 		// パーティクル
		// 		mParticleManager = std::make_unique<ParticleManager>();
		// 		mParticleManager->Init(
		// 			mRenderer.get(), mResourceManager->GetSrvManager()
		// 		);
		//
		// 		// ライン
		// 		mLineCommon = std::make_unique<LineCommon>();
		// 		mLineCommon->Init(mRenderer.get());
		//
		// 		DebugDraw::Init(mLineCommon.get());
		//
		// 		//---------------------------------------------------------------------
		// 		// すべての初期化が完了
		// 		//---------------------------------------------------------------------
		//
		// 		Console::SubmitCommand("neofetch");
		//
		// 		//---------------------------------------------------------------------
		// 		// エディターの初期化
		// 		//---------------------------------------------------------------------
		//
		// 		assert(SUCCEEDED(mRenderer->GetCommandList()->Close()));
		//
		// 		mConsoleSystem->ExecuteCommand(
		// 			"exec ./content/core/cfg/config_default.cfg"
		// 		);
		//
		// 		// ワールドの作成とシーンの読み込み
		// 		SwitchWorld<UWorld>();
		// 		mWorld->LoadSceneFromFile("./content/core/scenes/sandbox.json");

		return true;
	}

	/// @brief 更新
	void Engine::Tick() {
		mTimeSystem->BeginFrame();
		const float deltaTime = mTimeSystem->GetGameTime()->DeltaTime<float>();

		// 入力システムの更新
		mInputSystem->Update(deltaTime);
		// マウスカーソルのロックと表示状態の確認
		mInputSystem->CheckMouseCursorLockAndVisibility(
			mWindowManager->FindWindowById(
				mWindowManager->GetMainWindowId()
			)->GetHwnd()
		);
		//
		// #ifdef _DEBUG
		// 		ImGuiManager::NewFrame();
		// 		ImGuizmo::BeginFrame();
		// #endif

		/* ----------- 更新処理 ---------- */

		mConsoleSystem->Update(deltaTime);
		mTerminalSystem->Update(deltaTime);

		mRhiDevice->BeginFrame();

		mGraph->Execute(*mRhiDevice);

		mRhiDevice->EndFrame();

		//
		// #ifdef _DEBUG
		// 		Console::Update();
		// 		DebugHud::Update(deltaTime);
		// 		DebugDraw::Update();
		// #endif
		//
		// 		CameraManager::Update(deltaTime);
		//
		// 		if (mWorld) { mWorld->Tick(deltaTime); }
		//
		// 		auto offscreenTargets        = mRenderTargets.GetOffscreenPassTargets();
		// 		offscreenTargets.bClearColor =
		// 			ConVarManager::GetConVar("r_clear")->GetValueAsBool();
		// 		//---------------------------------------------------------------------
		// 		// --- PreRender↓ ---
		// 		mRenderer->PreRender();
		// 		//---------------------------------------------------------------------
		//
		// 		mRenderer->SetViewportAndScissor(
		// 			static_cast<uint32_t>(mRenderTargets.GetOffscreenRtv().rtv->
		// 			                                     GetDesc().Width),
		// 			mRenderTargets.GetOffscreenRtv().rtv->GetDesc().Height
		// 		);
		// 		mRenderer->BeginRenderPass(offscreenTargets);
		//
		// #ifdef _DEBUG
		// 		mLineCommon->Render();
		// 		DebugDraw::Draw();
		// #endif
		//
		// 		//if (mModeState) { mModeState->Render(*this); }
		//
		// 		// 先にバリアを設定
		// 		D3D12_RESOURCE_BARRIER barrier = {};
		// 		barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		// 		barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		// 		barrier.Transition.pResource   = mRenderTargets.GetOffscreenRtv().rtv.
		// 			Get();
		// 		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		// 		barrier.Transition.StateAfter  =
		// 			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		//
		// 		mRenderer->GetCommandList()->ResourceBarrier(1, &barrier);
		//
		// 		auto* radialBlur = mPostProcessPipeline.FindPass<PPRadialBlur>();
		// 		if (radialBlur) { radialBlur->SetBlurStrength(mBlurStrength); }
		//
		// 		// 最終出力先はモードで切り替え
		// 		D3D12_CPU_DESCRIPTOR_HANDLE finalOutRtv;
		// 		uint32_t                    finalW;
		// 		uint32_t                    finalH;
		// 		if (!mIsEditorMode) {
		// 			if (!mSwapchainPassBegun) {
		// 				mRenderer->BeginSwapChainRenderPass();
		// 				mSwapchainPassBegun = true;
		// 			}
		// 			finalOutRtv = mRenderer->GetSwapChainRenderTargetView();
		// 			auto window = mWindowManager->FindWindowById(
		// 				mWindowManager->GetMainWindowId()
		// 			);
		// 			auto desc = window->GetDesc();
		// 			finalW    = desc.width;
		// 			finalH    = desc.height;
		// 		} else {
		// 			finalOutRtv = mRenderTargets.GetPostProcessedRtv().rtvHandle;
		// 			finalW = static_cast<uint32_t>(mRenderTargets.GetOffscreenRtv().rtv
		// 				->GetDesc().Width);
		// 			finalH = mRenderTargets.GetOffscreenRtv().rtv->GetDesc().Height;
		// 		}
		//
		// 		mPostProcessPipeline.Execute(
		// 			mRenderer->GetCommandList(),
		// 			mRenderTargets.GetOffscreenRtv().rtv.Get(),
		// 			finalOutRtv,
		// 			finalW,
		// 			finalH
		// 		);
		//
		// 		if (mIsEditorMode && mResourceManager->GetSrvManager()) {
		// 			auto& postRtv = mRenderTargets.GetPostProcessedRtv();
		// 			mResourceManager->GetSrvManager()->CreateSRVForTexture2D(
		// 				postRtv.srvIndex,
		// 				postRtv.rtv.Get(),
		// 				postRtv.rtv->GetDesc().Format,
		// 				1
		// 			);
		// 			postRtv.srvHandleGPU = mResourceManager->GetSrvManager()->
		// 				GetGPUDescriptorHandle(
		// 					postRtv.srvIndex
		// 				);
		// 		}
		//
		// 		ImGui::Image(
		// 			mRenderTargets.GetPostProcessedRtv().srvHandleGPU.ptr,
		// 			ImVec2(static_cast<float>(finalW), static_cast<float>(finalH)),
		// 			ImVec2(0.0f, 0.0f),
		// 			ImVec2(1.0f, 1.0f)
		// 		);
		//
		// 		if (mInputSystem->IsHeld("forward")) {
		// 			Msg("Engine", "Forward key is held down.");
		// 		}
		//
		// 		//---------------------------------------------------------------------
		// 		// --- PostRender↓ ---
		// 		if (mIsEditorMode) { mRenderer->BeginSwapChainRenderPass(); }
		//
		// #ifdef _DEBUG
		// 		mImGuiManager->EndFrame();
		// #endif
		//
		// 		// バリアを元に戻す
		// 		barrier.Transition.StateBefore =
		// 			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		// 		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		// 		mRenderer->GetCommandList()->ResourceBarrier(1, &barrier);
		//
		// 		if (mIsEditorMode) {
		// 			// mPostProcessedRtv のバリアを戻す
		// 			D3D12_RESOURCE_BARRIER postBarrier = {};
		// 			postBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		// 			postBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		// 			postBarrier.Transition.pResource = mRenderTargets.
		// 			                                   GetPostProcessedRtv().rtv.Get();
		// 			postBarrier.Transition.StateBefore =
		// 				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		// 			postBarrier.Transition.StateAfter =
		// 				D3D12_RESOURCE_STATE_RENDER_TARGET;
		// 			mRenderer->GetCommandList()->ResourceBarrier(1, &postBarrier);
		// 		}
		//
		// 		mRenderer->PostRender();

		mTimeSystem->EndFrame();
	}

	/// @brief シャットダウン
	void Engine::Shutdown() {
		//mRenderer->WaitPreviousFrame();

		// 入力システムのリスナー解除
		mPlatformEvents->RemoveListener(mInputSystem.get());

		// 		DebugDraw::Shutdown();
		//
		// 		mParticleManager->Shutdown();
		// 		mParticleManager.reset();
		//
		// 		mRenderer->Shutdown();
		//
		// #ifdef _DEBUG
		// 		if (mImGuiManager) { mImGuiManager->Shutdown(); }
		// #endif
		//
		// 		mResourceManager->Shutdown();
		// 		mResourceManager.reset();

		mInputSystem->Shutdown();
		mConsoleSystem->Shutdown();
		mTimeSystem->Shutdown();

		SpecialMsg(
			LogLevel::Success,
			"Engine",
			"アリーヴェ帰ルチ! (さよナランチャ"
		);
	}

	/// @brief ウィンドウリサイズ時の処理
	/// @param width 幅
	/// @param height 高さ
	void Engine::OnResize(const uint32_t width, const uint32_t height) {
		if (width == 0 || height == 0) { return; }

		mRenderTargets.OnResize(width, height);
		mPostProcessPipeline.OnResize(width, height);
	}

	/// @brief オフスクリーンレンダーテクスチャのリサイズ
	/// @param width 幅
	/// @param height 高さ
	void Engine::ResizeOffscreenRenderTextures(
		const uint32_t width,
		const uint32_t height
	) {
		if (width == 0 || height == 0) { return; }

		mRenderTargets.OnResize(width, height);
		mPostProcessPipeline.OnResize(width, height);
	}

	/// @brief コンソールコマンドと変数の登録
	void Engine::RegisterConsoleCommandsAndVariables() {
		// コンソールコマンドを登録
		static UnnamedConCommand quit(
			"quit",
			[this](const std::vector<std::string>&) {
				mWishShutdown = true;
				return true;
			},
			"Quit the engine."
		);

#ifdef _DEBUG
		static UnnamedConCommand toggleeditor(
			"toggleeditor",
			[this](const std::vector<std::string>&) {
				mIsEditorMode = !mIsEditorMode;
				Warning(
					"Engine",
					"Editor mode is now {}",
					std::to_string(mIsEditorMode)
				);
				return true;
			},
			"Toggle editor mode."
		);
#endif

		// コンソール変数を登録
		mConsoleSystem->ExecuteCommand(
			"name " + WindowsUtils::GetWindowsUserName(), EXEC_FLAG::SILENT
		);
	}
}
