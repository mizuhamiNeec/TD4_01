#include "Engine.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <pch.h>

// ReSharper disable CppUnusedIncludeDirective
#include <engine/physics/core/Physics.h>
#include <engine/scene/Scene.h>
#include <engine/unnamed/subsystem/audio/AudioSystem.h>
// ReSharper restore CppUnusedIncludeDirective

#include <utility>

#include <core/assets/AssetManager.h>
#include <core/assets/loader/EventPresentationLoader.h>
#include <core/assets/loader/MaterialAssetLoader.h>
#include <core/assets/loader/MaterialInstanceAssetLoader.h>
#include <core/assets/loader/MeshAssetLoader.h>
#include <core/assets/loader/PostFxChainLoader.h>
#include <core/assets/loader/SequenceAssetLoader.h>
#include <core/assets/loader/ShaderProgramLoader.h>
#include <core/assets/loader/ShaderSourceLoader.h>
#include <core/assets/loader/SoundAssetLoader.h>
#include <core/assets/loader/TextureLoaderDirectXTex.h>
#include <core/assets/loader/UiDocumentAssetLoader.h>
#include <core/string/StrUtil.h>

#include <engine/editor/EditorRuntime.h>
#include <engine/Platform/PlatformEventsImpl.h>
#include <engine/Platform/WindowManager.h>
#include <engine/profiler/Profiler.h>
#include <engine/render/RenderModule.h>
#include <engine/render/frame/RenderFrameContext.h>
#include <engine/render/frame/RenderFrameInputs.h>
#include <engine/render/rendergraph/RenderPassContext.h>
#include <engine/rhi/RhiTypes.h>
#include <engine/rhi/d3d12/D3D12Device.h>
#include <engine/rhi/d3d12/D3D12Util.h>
#include <engine/rhi/interface/IRhiDevice.h>
#include <engine/sequence/SequenceRegressionRunner.h>
#include <engine/ui/ImGuiLayer.h>
#include <engine/unnamed/framework/entity/Entity.h>
#include <engine/unnamed/subsystem/console/concommand/ConCommand.h>
#include <engine/unnamed/subsystem/input/device/gamepad/GamepadDevice.h>
#include <engine/unnamed/subsystem/input/device/keyboard/KeyboardDevice.h>
#include <engine/unnamed/subsystem/input/device/mouse/MouseDevice.h>
#include <engine/unnamed/subsystem/interface/ServiceLocator.h>
#include <engine/unnamed/subsystem/terminal/TerminalSystem.h>
#include <engine/unnamed/subsystem/time/SystemClock.h>
#include <engine/unnamed/subsystem/time/TimeSystem.h>
#include <engine/world/EditorWorld.h>
#include <engine/world/GameWorld.h>
#include <engine/world/World.h>
#include <game/core/replay/DemoManager.h>

namespace Unnamed {
	namespace Rhi {
		class D3D12CommandContext;
		class D3D12Device;
	}

	Engine::Engine()  = default;
	Engine::~Engine() = default;

	int Engine::Run() {
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); // リークチェック
		[[maybe_unused]] HRESULT hr = CoInitializeEx(
			nullptr, COINIT_MULTITHREADED
		);
		timeBeginPeriod(1); // システムタイマーの分解能を上げる

		// 初期化
		if (!Init()) {
			UASSERT(false && "Failed to initialize Engine");
		}

		// メインループ
		while (true) {
			mWindowManager->ProcessMessage();

			// ウィンドウのリサイズ処理
			for (const WindowId id : mWindowManager->GetAllWindowIds()) {
				Window* wnd = mWindowManager->FindWindowById(id);
				if (!wnd) {
					continue;
				}
				if (const auto resize = wnd->ConsumeResizeEvent()) {
					if (
						resize->width > 0 && resize->height > 0 &&
						(std::cmp_not_equal(resize->width, mLastResizeWidth) ||
						 std::cmp_not_equal(resize->height, mLastResizeHeight))
					) {
						mLastResizeWidth = static_cast<uint32_t>(resize->width);
						mLastResizeHeight = static_cast<uint32_t>(resize->
							height);
						if (mRenderModule) {
							mRenderModule->OnResize(
								mLastResizeWidth, mLastResizeHeight
							);
						}
					}
				}
			}

			if (mWindowManager->ShouldQuit() || mWishShutdown) {
				break;
			}

			Tick();
		}

		// シャットダウン
		Shutdown();
		timeEndPeriod(1);
		CoUninitialize();
		return EXIT_SUCCESS;
	}

	void Engine::ToggleEditorScreenMode() const {
#ifdef _DEBUG
		if (mUEditorRuntime && mIsEditorMode) {
			mUEditorRuntime->TogglePresentMode();
		}
#endif
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
		if (!mWindowManager->Init(mConfig.window)) {
			return false;
		}

		// メインウィンドウのID取得
		const auto id = mWindowManager->GetMainWindowId();
		// メインウィンドウのポインタ取得
		const auto window = mWindowManager->FindWindowById(id);
		// HWND取得
		auto hwnd = window->GetHwnd();

		// ConsoleSystemの初期化
		mConsoleSystem = std::make_unique<ConsoleSystem>();
		if (!mConsoleSystem->Init()) {
			return false;
		}

		// TerminalSystem の初期化（ConsoleSystem 経由で操作するため Console を渡す）
		mTerminalSystem = std::make_unique<
			TerminalSystem>(mConsoleSystem.get());
		if (!mTerminalSystem->Init()) {
			return false;
		}

		mDemoManager = std::make_unique<DemoManager>();
		ServiceLocator::Register<DemoManager>(mDemoManager.get());

		mAssetManager = std::make_unique<AssetManager>();
		ServiceLocator::Register<AssetManager>(mAssetManager.get());

		// 各ローダーの登録
		mAssetManager->RegisterLoader(
			std::move(std::make_unique<TextureLoaderDirectXTex>())
		);
		mAssetManager->RegisterLoader(
			std::move(std::make_unique<MeshAssetLoader>())
		);
		mAssetManager->RegisterLoader(
			std::move(
				std::make_unique<ShaderProgramLoader>(mAssetManager.get())
			)
		);
		mAssetManager->RegisterLoader(
			std::move(
				std::make_unique<MaterialAssetLoader>(mAssetManager.get())
			)
		);
		mAssetManager->RegisterLoader(
			std::move(
				std::make_unique<MaterialInstanceAssetLoader>(
					mAssetManager.get()
				)
			)
		);
		mAssetManager->RegisterLoader(
			std::move(std::make_unique<PostFxChainLoader>(mAssetManager.get()))
		);
		mAssetManager->RegisterLoader(
			std::move(std::make_unique<ShaderSourceLoader>(mAssetManager.get()))
		);
		mAssetManager->RegisterLoader(
			std::move(std::make_unique<UiDocumentAssetLoader>())
		);
		mAssetManager->RegisterLoader(
			std::move(std::make_unique<SoundAssetLoader>())
		);
		mAssetManager->RegisterLoader(
			std::move(std::make_unique<EventPresentationLoader>())
		);
		mAssetManager->RegisterLoader(
			std::move(std::make_unique<SequenceAssetLoader>())
		);

		mAudioSystem = std::make_unique<AudioSystem>();
		if (!mAudioSystem->Init()) {
			return false;
		}
		ServiceLocator::Register<AudioSystem>(mAudioSystem.get());

		// TimeSystemの初期化
		mTimeSystem = std::make_unique<TimeSystem>();
		if (!mTimeSystem->Init()) {
			return false;
		}

		mProfiler = std::make_unique<Profiler>();
		ServiceLocator::Register<Profiler>(mProfiler.get());

		// InputSystemの初期化
		mInputSystem = std::make_unique<InputSystem>();
		if (!mInputSystem->Init()) {
			return false;
		}

		// デバイス登録
		const auto keyboardDevice = std::make_shared<KeyboardDevice>(hwnd);
		const auto mouseDevice    = std::make_shared<MouseDevice>(hwnd);
		const auto gamepadDevice  = std::make_shared<GamepadDevice>(hwnd);
		mInputSystem->RegisterDevice(keyboardDevice);
		mInputSystem->RegisterDevice(mouseDevice);
		mInputSystem->RegisterDevice(gamepadDevice);

		// コンソールコマンドと変数の登録
		mConsoleSystem->ExecuteCommand(
			"exec ./content/core/cfg/config_default.cfg"
		);
		mConsoleSystem->ExecuteCommand(
			"exec ./content/core/cfg/user.cfg"
		);

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
			.bufferCount = 2, // ダブルバッファリング
			.format      = Rhi::TEXTURE_FORMAT::R8G8B8A8_UNORM,
			.vSync       = false
		};

		mRhiDevice = Rhi::CreateD3D12Device(
			hwnd,
			deviceDesc,
			swapChainDesc
		);

		mRenderModule = std::make_unique<Render::RenderModule>(
			*mAssetManager, *mRhiDevice
		);
		mRenderModule->Init(mConsoleSystem.get());
		mRenderFrameContext = std::make_unique<Render::RenderFrameContext>();

#ifdef _DEBUG
		auto& dx     = dynamic_cast<Rhi::D3D12Device&>(*mRhiDevice);
		mUImGuiLayer = std::make_unique<ImGuiLayer>(
			hwnd,
			dx,
			dx.GetSwapChain().GetBufferCount(),
			Rhi::ToDxgiFormat(dx.GetSwapChain().GetFormat())
		);

		mRenderModule->SetUiCallbacks(
			[this](const Render::RenderPassContext& passContext) {
				if (mUImGuiLayer) {
					mUImGuiLayer->RenderMainDrawData(passContext);
				}
			},
			[this] {
				if (mUImGuiLayer) {
					mUImGuiLayer->RenderPlatformWindows();
				}
			}
		);
#endif

		RegisterConsoleCommandsAndVariables();

		if (mConfig.mode == RUN_MODE::EDITOR) {
#ifdef _DEBUG
			mUEditorRuntime = std::make_unique<EditorRuntime>(
				mConsoleSystem.get(),
				mInputSystem.get(),
				mAssetManager.get(),
				mDemoManager.get(),
				mProfiler.get(),
				*mWindowManager,
				*mRenderModule,
				*mUImGuiLayer
			);
			if (World* runtimeWorld = mUEditorRuntime->GetRuntimeWorld()) {
				runtimeWorld->LoadSceneFromFile(
					"./content/parkour/scenes/title.json"
				);
			}

			mConsoleSystem->ExecuteCommand(
				"exec ./content/core/cfg/editor.cfg"
			);
#endif
		} else {
			auto& world = SwitchWorld<GameWorld>();
			world.LoadSceneFromFile("./content/parkour/scenes/title.json");
		}

		// ユーザー名をコンソール変数に設定
		mConsoleSystem->ExecuteCommand(
			"name " + WindowsUtils::GetWindowsUserName(), EXEC_FLAG::SILENT
		);

		return true;
	}

	/// @brief 更新
	void Engine::Tick() {
		const auto frameStart = std::chrono::steady_clock::now();
		mTimeSystem->BeginFrame();
		const float unscaledDeltaTime = mTimeSystem->GetGameTime()->DeltaTime<
			float>();
		const float scaledDeltaTime = mTimeSystem->GetGameTime()->
		                                           ScaledDeltaTime<float>();

		// プロファイラのフレーム開始
		if (mProfiler) {
			mProfiler->BeginFrame();
		}

		// 入力システムの更新
		{
			Profiler::ScopeTimer scope(mProfiler.get(), "Input.Update");
			mInputSystem->Update(unscaledDeltaTime);
		}

		// アセットのホットリロードのポーリング
		{
			mAssetHotReloadPollAccumulator    += unscaledDeltaTime;
			const float hotreloadpollinterval = mConsoleSystem->
				GetConVarValueOr(
					"asset_hotreloadpollinterval",
					0.25f
				);

			if (
				mAssetManager &&
				mAssetHotReloadPollAccumulator >=
				hotreloadpollinterval
			) {
				Profiler::ScopeTimer scope(
					mProfiler.get(), "Asset.PollHotReload"
				);
				mAssetManager->PollSourceChanges();
				mAssetHotReloadPollAccumulator = 0.0f;
			}
		}

		// マウスカーソルのロックと表示状態の確認
		{
			Profiler::ScopeTimer scope(mProfiler.get(), "Input.MouseLock");
			mInputSystem->CheckMouseCursorLockAndVisibility(
				mWindowManager->FindWindowById(
					mWindowManager->GetMainWindowId()
				)->GetHwnd()
			);
		}

#ifdef _DEBUG
		// Update内でImGuiを使えるように更新前にフレーム開始
		if (mUImGuiLayer) {
			Profiler::ScopeTimer scope(mProfiler.get(), "ImGui.BeginFrame");
			auto& dx = dynamic_cast<Rhi::D3D12Device&>(*mRhiDevice);
			mUImGuiLayer->BeginFrame(dx.GetCurrentFrameIndex());
		}
		if (mUEditorRuntime && mIsEditorMode) {
			if (
				mUEditorRuntime->GetPresentMode() ==
				EDITOR_PRESENT_MODE::VIEWPORT_PANEL
			) {
				Profiler::ScopeTimer scope(mProfiler.get(), "Editor.BeginUI");
				mUEditorRuntime->BeginUI();
			}
		}
#endif

		/* ----------- 更新処理 ---------- */

		{
			Profiler::ScopeTimer scope(mProfiler.get(), "Console.Update");
			mConsoleSystem->Update(unscaledDeltaTime);
		}
		{
			Profiler::ScopeTimer scope(mProfiler.get(), "Terminal.Update");
			mTerminalSystem->Update(unscaledDeltaTime);
		}

		World* runtimeWorld = mWorld.get();
#ifdef _DEBUG
		if (mUEditorRuntime && mIsEditorMode) {
			runtimeWorld = mUEditorRuntime->GetRuntimeWorld();
		}
#endif

		// ワールドの固定シミュレーション更新 + 描画フレーム更新
		if (runtimeWorld) {
			{
				// シーン遷移はフレーム先頭でまとめて適用し、更新ループ中の差し替えを避けます。
				Profiler::ScopeTimer scope(
					mProfiler.get(), "World.ProcessPendingSceneTransition"
				);
				if (World* transitionTarget = ResolveSceneTransitionTargetWorld(
					runtimeWorld
				)) {
					transitionTarget->ProcessPendingSceneTransition();
				}
			}

			static constexpr uint32_t kMaxFixedTicksPerFrame = 16u;

			const uint32_t tickRate = mDemoManager ?
				                          mDemoManager->
				                          GetSimulationTickRate() :
				                          DemoManager::ResolveConfiguredTickRate();
			const float fixedStepSeconds = DemoManager::TickStepSecondsFromRate(
				tickRate
			);

			if (mDemoManager &&
			    (mDemoManager->IsPlayback() || mDemoManager->IsRecording())) {
				const uint32_t configuredTickRate =
					DemoManager::ResolveConfiguredTickRate();
				if (configuredTickRate != tickRate &&
				    configuredTickRate !=
				    mLastLoggedTickrateMismatchConfigured) {
					Warning(
						"Engine",
						"sv_tickrate={} is ignored while demo mode is active. Using tickrate={}.",
						configuredTickRate,
						tickRate
					);
					mLastLoggedTickrateMismatchConfigured = configuredTickRate;
				}
			} else {
				mLastLoggedTickrateMismatchConfigured = 0;
			}

			mSimulationAccumulator += std::max(0.0f, scaledDeltaTime);
			mSimulationAccumulator = std::min(
				mSimulationAccumulator,
				fixedStepSeconds * static_cast<float>(kMaxFixedTicksPerFrame)
			);

			{
				Profiler::ScopeTimer scope(
					mProfiler.get(), "World.FrameInputTick"
				);
				runtimeWorld->FrameInputTick(unscaledDeltaTime);
			}

			{
				Profiler::ScopeTimer scope(mProfiler.get(), "World.FixedTick");
				uint32_t             fixedTickCount = 0;
				while (
					mSimulationAccumulator >= fixedStepSeconds &&
					fixedTickCount < kMaxFixedTicksPerFrame
				) {
					runtimeWorld->FixedTick(fixedStepSeconds);
					mSimulationAccumulator -= fixedStepSeconds;
					++fixedTickCount;
				}
			}

			{
				const float interpolationAlpha =
					fixedStepSeconds > 0.0f ?
						std::clamp(
							mSimulationAccumulator / fixedStepSeconds,
							0.0f,
							1.0f
						) :
						0.0f;
				Profiler::ScopeTimer scope(mProfiler.get(), "World.RenderTick");
				runtimeWorld->RenderTick(scaledDeltaTime, interpolationAlpha);
			}
		}

		Render::RenderFrameInputs inputs = {};
		// フレームインデックスとゲーム時間を設定
		inputs.frameIndex = mFrameIndex++;
		inputs.time       =
			static_cast<float>(mTimeSystem->GetGameTime()->TotalTime());
		if (runtimeWorld && mRenderFrameContext) {
			Profiler::ScopeTimer scope(
				mProfiler.get(), "World.FillRenderFrameInputs"
			);
			runtimeWorld->FillRenderFrameInputs(
				inputs, *mRenderFrameContext, *mAssetManager
			);
		}

#ifdef _DEBUG
		if (mUImGuiLayer) {
			if (mUEditorRuntime && mIsEditorMode) {
				mUEditorRuntime->SyncViewOutputs();
				if (
					mUEditorRuntime->GetPresentMode() ==
					EDITOR_PRESENT_MODE::VIEWPORT_PANEL
				) {
					Profiler::ScopeTimer scope(
						mProfiler.get(), "Editor.BuildUi"
					);
					mUEditorRuntime->BuildUi(unscaledDeltaTime);
				}
			}
			{
				Profiler::ScopeTimer scope(mProfiler.get(), "ImGui.EndFrame");
				mUImGuiLayer->EndFrame();
			}
		}
		if (mUEditorRuntime && mIsEditorMode) {
			mUEditorRuntime->FillEditorRenderViews(inputs);
		}
#endif

		{
			Profiler::ScopeTimer scope(mProfiler.get(), "Render.Tick");
			mRenderModule->Tick(inputs);
		}

		if (mProfiler) {
			const float totalMs = std::chrono::duration<float, std::milli>(
				std::chrono::steady_clock::now() - frameStart
			).count();
			mProfiler->AddSample("Frame.Total", totalMs);
			mProfiler->EndFrame();
		}

		mTimeSystem->EndFrame(); // フレーム終了
	}

	/// @brief シャットダウン
	void Engine::Shutdown() {
		mQuitCommand.reset();
		mMapCommand.reset();
		mReloadSceneCommand.reset();
		mPostFxSetCommand.reset();
		mPostFxEnableCommand.reset();
		mPostFxClearParamCommand.reset();
		mPostFxClearPassCommand.reset();
		mPostFxResetCommand.reset();
		mPostFxListCommand.reset();
		mPostFxChainCommand.reset();
		mPostFxChainReloadCommand.reset();
		mSequenceRegressionRunCommand.reset();
#ifdef _DEBUG
		mToggleEditorCommand.reset();
		mToggleFullscreenCommand.reset();
#endif

		if (mWorld) {
			mWorld->Shutdown();
			mWorld.reset();
		}
		ServiceLocator::Register<DemoManager>(nullptr);
		mDemoManager.reset();

#ifdef _DEBUG
		if (mRenderModule) {
			mRenderModule->SetUiCallbacks({}, {});
		}
		mUEditorRuntime.reset();
		mUImGuiLayer.reset();
#endif

		mRenderFrameContext.reset();
		mRenderModule.reset();
		mRhiDevice.reset();
		mProfiler.reset();
		if (mAudioSystem) {
			mAudioSystem->Shutdown();
			mAudioSystem.reset();
		}
		ServiceLocator::Register<AudioSystem>(nullptr);

		// 入力システムのリスナー解除
		if (mPlatformEvents && mInputSystem) {
			mPlatformEvents->RemoveListener(mInputSystem.get());
		}

		if (mInputSystem) {
			mInputSystem->Shutdown();
		}
		if (mTerminalSystem) {
			mTerminalSystem->Shutdown();
		}

		SpecialMsg(
			LogLevel::Success,
			"Engine",
			"アリーヴェ帰ルチ! (さよナランチャ"
		);

		if (mConsoleSystem) {
			mConsoleSystem->Shutdown();
		}
		if (mTimeSystem) {
			mTimeSystem->Shutdown();
		}
		if (mWindowManager) {
			mWindowManager->Shutdown();
		}
	}

	/// @brief コンソールコマンドと変数の登録
	void Engine::RegisterConsoleCommandsAndVariables() {
		auto TryParseFloat = [](const std::string_view text, float& outValue) {
			const char* begin     = text.data();
			const char* end       = text.data() + text.size();
			auto        [ptr, ec] = std::from_chars(begin, end, outValue);
			if (ec == std::errc() && ptr == end) {
				return true;
			}
			try {
				size_t consumed = 0;
				outValue        = std::stof(std::string(text), &consumed);
				return consumed == text.size();
			} catch (...) {
				return false;
			}
		};

		auto ParseBool = [](const std::string_view text, bool& outValue) {
			const std::string lower = StrUtil::ToLowerCase(std::string(text));
			if (
				lower == "1" ||
				lower == "true" ||
				lower == "on" ||
				lower == "yes"
			) {
				outValue = true;
				return true;
			}
			if (
				lower == "0" ||
				lower == "false" ||
				lower == "off" ||
				lower == "no"
			) {
				outValue = false;
				return true;
			}
			return false;
		};

		auto ParseVec4 = [&](const std::vector<std::string>& args, Vec4& outValue) {
			if (args.size() != 4) {
				return false;
			}
			float values[4] = {};
			for (size_t i = 0; i < 4; ++i) {
				if (!TryParseFloat(args[i], values[i])) {
					return false;
				}
			}
			outValue = Vec4(values[0], values[1], values[2], values[3]);
			return true;
		};

		const auto queueSceneTransition = [this](const std::string& rawPath) {
			World* runtimeWorld = GetWorld();
			if (!runtimeWorld) {
				Warning("Engine", "Scene transition failed: runtime world is null.");
				return false;
			}

			World* transitionTarget = ResolveSceneTransitionTargetWorld(
				runtimeWorld
			);
			if (!transitionTarget) {
				Warning("Engine", "Scene transition failed: transition target world is null.");
				return false;
			}

			const std::string normalizedPath = StrUtil::NormalizePath(
				StrUtil::TrimSpaces(rawPath)
			);
			if (normalizedPath.empty()) {
				Warning("Engine", "Scene transition failed: path is empty.");
				return false;
			}

			transitionTarget->RequestSceneTransition(normalizedPath);
			return true;
		};

		mQuitCommand = std::make_unique<ConCommand>(
			"quit",
			[this](const std::vector<std::string>&) {
				mWishShutdown = true;
				return true;
			},
			"Quit the engine."
		);

		mMapCommand = std::make_unique<ConCommand>(
			"map",
			[queueSceneTransition](const std::vector<std::string>& args) {
				if (args.empty()) {
					Warning("Engine", "Usage: map <scenePath>");
					return false;
				}

				// 引数を1つのパスとして扱い、空白を含むケースも吸収します。
				return queueSceneTransition(StrUtil::Join(args, " "));
			},
			"Queue a scene transition. Usage: map <scenePath>"
		);

		mReloadSceneCommand = std::make_unique<ConCommand>(
			"reloadscene",
			[this, queueSceneTransition](const std::vector<std::string>&) {
				World* runtimeWorld = GetWorld();
				if (!runtimeWorld) {
					Warning("Engine", "Reload failed: runtime world is null.");
					return false;
				}

				World* transitionTarget = ResolveSceneTransitionTargetWorld(
					runtimeWorld
				);
				if (!transitionTarget) {
					Warning("Engine", "Reload failed: transition target world is null.");
					return false;
				}

				const std::string loadedPath = std::string(
					transitionTarget->GetLoadedScenePath()
				);
				if (loadedPath.empty()) {
					Warning("Engine", "Reload failed: no loaded scene path.");
					return false;
				}

				return queueSceneTransition(loadedPath);
			},
			"Reload current scene."
		);

		mSequenceRegressionRunCommand = std::make_unique<ConCommand>(
			"seq_regression_run",
			[this](const std::vector<std::string>&) {
				if (!mAssetManager) {
					Warning("SeqRegression", "AssetManager is null.");
					return false;
				}
				World* world = GetWorld();
				if (!world) {
					Warning("SeqRegression", "World is null.");
					return false;
				}

				std::string report = {};
				const bool passed = SequenceRegressionRunner::RunAll(
					*world,
					*mAssetManager,
					&report
				);
				if (passed) {
					Msg("SeqRegression", "\n{}", report);
				} else {
					Warning("SeqRegression", "\n{}", report);
				}
				return passed;
			},
			"Run fixed-tick regression tests for sequence runtime."
		);
#ifdef _DEBUG
		mToggleEditorCommand = std::make_unique<ConCommand>(
			"toggleeditor",
			[this](const std::vector<std::string>&) {
				ToggleEditorScreenMode();
				return true;
			},
			"Toggle editor mode."
		);

		mToggleFullscreenCommand = std::make_unique<ConCommand>(
			"togglefullscreen",
			[this](const std::vector<std::string>&) {
				ToggleFullscreen();
				return true;
			},
			"Toggle editor viewport/swapchain presentation mode."
		);
#endif
	}

	World* Engine::ResolveSceneTransitionTargetWorld(World* runtimeWorld) const {
#ifdef _DEBUG
		if (auto* editorWorld = dynamic_cast<EditorWorld*>(runtimeWorld)) {
			return editorWorld->GetRuntimeSceneWorld();
		}
#endif
		return runtimeWorld;
	}

	void Engine::ToggleFullscreen() const {
		if (mWindowManager) {
			if (const Window* window = mWindowManager->FindWindowById(
				mWindowManager->GetMainWindowId()
			)) {
				window->ToggleFullscreen();
			}
		}
	}

	template <class TWorld, class... Args>
	TWorld& Engine::SwitchWorld(Args&&... args) {
		static_assert(std::is_base_of_v<World, TWorld>);

		if (mWorld) {
			mWorld->Shutdown();
			mWorld.reset();
		}

		auto newWorld = std::make_unique<TWorld>(
			std::forward<Args>(args)...
		);
		TWorld* raw = newWorld.get();
		raw->SetServices(
			{
				.console      = mConsoleSystem.get(),
				.inputSystem  = mInputSystem.get(),
				.profiler     = mProfiler.get(),
				.assetManager = mAssetManager.get(),
				.demoManager  = mDemoManager.get(),
				.audioSystem  = mAudioSystem.get()
			}
		);

		mWorld = std::move(newWorld);

		mSimulationAccumulator = 0.0f;

		mWorld->Initialize();
		return *raw;
	}

	World* Engine::GetWorld() const {
#ifdef _DEBUG
		if (mUEditorRuntime && mIsEditorMode) {
			return mUEditorRuntime->GetRuntimeWorld();
		}
#endif
		return mWorld.get();
	}
}
