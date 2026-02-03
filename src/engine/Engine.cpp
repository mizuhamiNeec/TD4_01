#include <pch.h>

#ifdef _DEBUG
#include <imgui_internal.h>
// ImGuizmoのインクルードはImGuiより後! いいね!?
#include <ImGuizmo.h>
#endif

#include <engine/Engine.h>
#include <engine/Camera/CameraManager.h>
#include <engine/Debug/DebugDraw.h>
#include <engine/Debug/DebugHud.h>
#include <engine/ImGui/ImGuiWidgets.h>
#include <engine/Input/InputSystem.h>
#include <engine/OldConsole/ConCommand.h>
#include <engine/OldConsole/ConVarManager.h>
#include <engine/Platform/WindowsUtils.h>
#include <engine/postprocess/PostProcessPipeline.h>
#include <engine/postprocess/PPBloom.h>
#include <engine/postprocess/PPChromaticAberration.h>
#include <engine/postprocess/PPRadialBlur.h>
#include <engine/postprocess/PPVignette.h>
#include <engine/ResourceSystem/Audio/AudioManager.h>
#include <engine/unnamed/subsystem/console/ConsoleScriptParser.h>
#include <engine/unnamed/subsystem/console/ConsoleSystem.h>
#include <engine/unnamed/subsystem/console/concommand/UnnamedConCommand.h>
#include <engine/unnamed/subsystem/console/concommand/UnnamedConVar.h>
#include <engine/unnamed/subsystem/input/device/keyboard/KeyboardDevice.h>
#include <engine/unnamed/subsystem/input/device/mouse/MouseDevice.h>
#include <engine/unnamed/subsystem/interface/ServiceLocator.h>
#include <engine/unnamed/subsystem/time/TimeSystem.h>

namespace Unnamed {
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
					OnResize(resize->width, resize->height);
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

	/// @brief 初期化
	/// @return 成功したらtrueを返す
	bool Engine::Init() {
		ServiceLocator::Register<Engine>(this);

		mConfig = {
#ifdef _DEBUG
			.mode = ENGINE_MODE::EDITOR,
#else
			.mode = ENGINE_MODE::GAME,
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

		mRenderer = std::make_unique<D3D12>(hwnd, window->GetDesc());

		InputSystem::Init();

		RegisterConsoleCommandsAndVariables();

		// 各マネージャーの初期化
		mResourceManager = std::make_unique<ResourceManager>(mRenderer.get());

		mResourceManager->Init();
		mRenderer->SetShaderResourceViewManager(
			mResourceManager->GetSrvManager()
		);
		mRenderer->Init();

		mAudioManager = std::make_unique<AudioManager>();
		mAudioManager->Init();

#ifdef _DEBUG
		// ImGuiFontテクスチャ用にSRVを確保
		mResourceManager->GetSrvManager()->AllocateForTexture2D();
		// ImGuiManagerの初期化
		mImGuiManager = std::make_unique<ImGuiManager>(
			hwnd, mRenderer.get(), mResourceManager->GetSrvManager()
		);
#endif

		// コンソールを作成
		mConsole = std::make_unique<Console>();

#pragma region PostProcessInit
		mRenderTargets.Init(
			mRenderer.get(),
			window->GetDesc().width,
			window->GetDesc().height,
			kOffscreenClearColor,
			kBufferFormat,
			DXGI_FORMAT_D32_FLOAT
		);

		mPostProcessPipeline.Init(
			mRenderer.get(),
			mResourceManager->GetSrvManager(),
			window->GetDesc().width,
			window->GetDesc().height,
			reinterpret_cast<const float*>(&kOffscreenClearColor),
			kBufferFormat,
			DXGI_FORMAT_D32_FLOAT
		);

		mPostProcessPipeline.AddPass(
			std::make_unique<PPBloom>(
				mRenderer->GetDevice(), mResourceManager->GetSrvManager()
			)
		);

		mPostProcessPipeline.AddPass(
			std::make_unique<PPVignette>(
				mRenderer->GetDevice(), mResourceManager->GetSrvManager()
			)
		);

		mPostProcessPipeline.AddPass(
			std::make_unique<PPChromaticAberration>(
				mRenderer->GetDevice(), mResourceManager->GetSrvManager()
			)
		);

		mPostProcessPipeline.AddPass(
			std::make_unique<PPRadialBlur>(
				mRenderer->GetDevice(), mResourceManager->GetSrvManager()
			)
		);
#pragma endregion

		// スプライト
		mSpriteCommon = std::make_unique<SpriteCommon>();
		mSpriteCommon->Init(mRenderer.get());

		// パーティクル
		mParticleManager = std::make_unique<ParticleManager>();
		mParticleManager->Init(
			mRenderer.get(), mResourceManager->GetSrvManager()
		);

		// ライン
		mLineCommon = std::make_unique<LineCommon>();
		mLineCommon->Init(mRenderer.get());

		DebugDraw::Init(mLineCommon.get());

		//---------------------------------------------------------------------
		// すべての初期化が完了
		//---------------------------------------------------------------------

		Console::SubmitCommand("neofetch");

		//---------------------------------------------------------------------
		// エディターの初期化
		//---------------------------------------------------------------------

		assert(SUCCEEDED(mRenderer->GetCommandList()->Close()));

		mConsoleSystem->ExecuteCommand(
			"exec ./content/core/cfg/config_default.cfg"
		);

		// ワールドの作成とシーンの読み込み
		SwitchWorld<UWorld>();
		mWorld->LoadSceneFromFile("./content/core/scenes/sandbox.json");

		return true;
	}

	/// @brief 更新
	void Engine::Tick() {
		mTimeSystem->BeginFrame();
		float deltaTime = mTimeSystem->GetGameTime()->DeltaTime<float>();

		mInputSystem->Update(deltaTime);

#ifdef _DEBUG
		ImGuiManager::NewFrame();
		ImGuizmo::BeginFrame();
#endif

		/* ----------- 更新処理 ---------- */

		mConsoleSystem->Update(deltaTime);

		InputSystem::Update();

#ifdef _DEBUG
		Console::Update();
		DebugHud::Update(mTimeSystem->GetGameTime()->ScaledDeltaTime<float>());
		DebugDraw::Update();
#endif

		CameraManager::Update(
			mTimeSystem->GetGameTime()->ScaledDeltaTime<float>()
		);

		if (mWorld) { mWorld->Tick(deltaTime); }

		auto offscreenTargets        = mRenderTargets.GetOffscreenPassTargets();
		offscreenTargets.bClearColor =
			ConVarManager::GetConVar("r_clear")->GetValueAsBool();
		//---------------------------------------------------------------------
		// --- PreRender↓ ---
		mRenderer->PreRender();
		//---------------------------------------------------------------------

		mRenderer->SetViewportAndScissor(
			static_cast<uint32_t>(mRenderTargets.GetOffscreenRtv().rtv->
			                                     GetDesc().Width),
			mRenderTargets.GetOffscreenRtv().rtv->GetDesc().Height
		);
		mRenderer->BeginRenderPass(offscreenTargets);

#ifdef _DEBUG
		mLineCommon->Render();
		DebugDraw::Draw();
#endif

		//if (mModeState) { mModeState->Render(*this); }

		// 先にバリアを設定
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource   = mRenderTargets.GetOffscreenRtv().rtv.
			Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter  =
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		mRenderer->GetCommandList()->ResourceBarrier(1, &barrier);

		auto* radialBlur = mPostProcessPipeline.FindPass<PPRadialBlur>();
		if (radialBlur) { radialBlur->SetBlurStrength(mBlurStrength); }

		// 最終出力先はモードで切り替え
		D3D12_CPU_DESCRIPTOR_HANDLE finalOutRtv;
		uint32_t                    finalW;
		uint32_t                    finalH;
		if (!mIsEditorMode) {
			if (!mSwapchainPassBegun) {
				mRenderer->BeginSwapChainRenderPass();
				mSwapchainPassBegun = true;
			}
			finalOutRtv = mRenderer->GetSwapChainRenderTargetView();
			auto window = mWindowManager->FindWindowById(
				mWindowManager->GetMainWindowId()
			);
			auto desc = window->GetDesc();
			finalW    = desc.width;
			finalH    = desc.height;
		} else {
			finalOutRtv = mRenderTargets.GetPostProcessedRtv().rtvHandle;
			finalW = static_cast<uint32_t>(mRenderTargets.GetOffscreenRtv().rtv
				->GetDesc().Width);
			finalH = mRenderTargets.GetOffscreenRtv().rtv->GetDesc().Height;
		}

		mPostProcessPipeline.Execute(
			mRenderer->GetCommandList(),
			mRenderTargets.GetOffscreenRtv().rtv.Get(),
			finalOutRtv,
			finalW,
			finalH
		);

		if (mIsEditorMode && mResourceManager->GetSrvManager()) {
			auto& postRtv = mRenderTargets.GetPostProcessedRtv();
			mResourceManager->GetSrvManager()->CreateSRVForTexture2D(
				postRtv.srvIndex,
				postRtv.rtv.Get(),
				postRtv.rtv->GetDesc().Format,
				1
			);
			postRtv.srvHandleGPU = mResourceManager->GetSrvManager()->
				GetGPUDescriptorHandle(
					postRtv.srvIndex
				);
		}

		//---------------------------------------------------------------------
		// --- PostRender↓ ---
		if (mIsEditorMode) { mRenderer->BeginSwapChainRenderPass(); }

#ifdef _DEBUG
		mImGuiManager->EndFrame();
#endif

		// バリアを元に戻す
		barrier.Transition.StateBefore =
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		mRenderer->GetCommandList()->ResourceBarrier(1, &barrier);

		if (mIsEditorMode) {
			// mPostProcessedRtv のバリアを戻す
			D3D12_RESOURCE_BARRIER postBarrier = {};
			postBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			postBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			postBarrier.Transition.pResource = mRenderTargets.
			                                   GetPostProcessedRtv().rtv.Get();
			postBarrier.Transition.StateBefore =
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			postBarrier.Transition.StateAfter =
				D3D12_RESOURCE_STATE_RENDER_TARGET;
			mRenderer->GetCommandList()->ResourceBarrier(1, &postBarrier);
		}

		mRenderer->PostRender();

		mTimeSystem->EndFrame();
	}

	/// @brief シャットダウン
	void Engine::Shutdown() {
		mRenderer->WaitPreviousFrame();

		DebugDraw::Shutdown();

		mParticleManager->Shutdown();
		mParticleManager.reset();

		mRenderer->Shutdown();

#ifdef _DEBUG
		if (mImGuiManager) { mImGuiManager->Shutdown(); }
#endif

		mResourceManager->Shutdown();
		mResourceManager.reset();

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
