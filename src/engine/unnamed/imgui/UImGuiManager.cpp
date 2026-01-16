#include "UImGuiManager.h"
#ifdef _DEBUG
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include "core/unnamed/UnnamedMacro.h"
#include "core/unnamed/json/JsonReader.h"
#include "core/unnamed/json/JsonWriter.h"

#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/concommand/UnnamedConVar.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/unnamed/urenderer/GraphicsDevice.h"

namespace Unnamed {
	static constexpr float   kDefaultFontSize = 14.0f;
	static constexpr ImWchar kIconRanges[]    = {0xe003, 0xf8ff, 0};

	UImGuiManager::UImGuiManager(GraphicsDevice* graphicsDevice, void* hWnd) :
		mGraphicsDevice(graphicsDevice) {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io    = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;

		auto console = ServiceLocator::Get<ConsoleSystem>();

		auto configpath    = console->GetConVar("im_configpath");
		auto configpathVar = dynamic_cast<UnnamedConVar<std::string>*>(
			configpath);

		JsonReader reader(configpathVar->GetValue());

		ImFontConfig imFontConfig;
		if (reader.Has("fontconfig")) {
			auto fontconfig = reader["fontconfig"];
			{
				if (fontconfig.Has("OversampleH")) {
					imFontConfig.OversampleH =
						static_cast<ImS8>(fontconfig["OversampleH"].GetInt());
				}
				if (fontconfig.Has("OversampleV")) {
					imFontConfig.OversampleV =
						static_cast<ImS8>(fontconfig["OversampleV"].GetInt());
				}
				if (fontconfig.Has("PixelSnapH")) {
					imFontConfig.PixelSnapH =
						fontconfig["PixelSnapH"].GetBool();
				}
				if (fontconfig.Has("SizePixels")) {
					imFontConfig.SizePixels =
						fontconfig["SizePixels"].GetFloat();
				}
				if (fontconfig.Has("GlyphMinAdvanceX")) {
					imFontConfig.GlyphMinAdvanceX =
						fontconfig["GlyphMinAdvanceX"].GetFloat();
				}
			}
		}

		if (reader.Has("fontpath")) {
			auto path = reader["fontpath"];
			{
				if (path.Has("default")) {
					auto def     = path["default"];
					auto defpath = def["path"].GetString();
					io.Fonts->AddFontFromFileTTF(
						defpath.c_str(),
						def.Has("size") ?
							def["size"].GetFloat() :
							kDefaultFontSize,
						&imFontConfig,
						io.Fonts->GetGlyphRangesDefault()
					);
				}
				if (path.Has("japanese")) {
					imFontConfig.MergeMode = true;
					auto ja                = path["japanese"];
					auto jaPath            = ja["path"].GetString();
					io.Fonts->AddFontFromFileTTF(
						jaPath.c_str(),
						ja.Has("size") ?
							ja["size"].GetFloat() :
							kDefaultFontSize,
						&imFontConfig,
						io.Fonts->GetGlyphRangesJapanese()
					);
				}
				if (path.Has("icon")) {
					auto icon     = path["icon"];
					auto iconPath = icon["path"].GetString();

					// ??? 何故かアイコンフォントのベースラインがずれるので補正
					imFontConfig.GlyphOffset = ImVec2(0.0f, 2.0f);
					io.Fonts->AddFontFromFileTTF(
						iconPath.c_str(),
						icon.Has("size") ?
							icon["size"].GetFloat() :
							kDefaultFontSize,
						&imFontConfig,
						kIconRanges
					);
				}
			}
		}

		ImGui_ImplWin32_Init(hWnd);

		ImGui_ImplDX12_InitInfo initInfo = {};
		initInfo.Device                  = mGraphicsDevice->Device();
		initInfo.NumFramesInFlight       = kFrameBufferCount;
		initInfo.RTVFormat               = kBufferFormat;
		initInfo.CommandQueue            = mGraphicsDevice->GetCommandQueue();
		initInfo.SrvDescriptorHeap       = mGraphicsDevice->GetSrvAllocator()->
			GetHeap();
		initInfo.UserData = this;

		initInfo.SrvDescriptorAllocFn = [](
			ImGui_ImplDX12_InitInfo*     info,
			D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
			D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle
		) {
				const auto mgr   = static_cast<UImGuiManager*>(info->UserData);
				const auto index = mgr->GetSrvAllocator()->Allocate();
				*cpuHandle       = mgr->GetSrvAllocator()->CPUHandle(index);
				*gpuHandle       = mgr->GetSrvAllocator()->GPUHandle(index);
			};

		initInfo.SrvDescriptorFreeFn = [](
			ImGui_ImplDX12_InitInfo*    info,
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
			D3D12_GPU_DESCRIPTOR_HANDLE) {
				const auto mgr = static_cast<UImGuiManager*>(info->UserData);
				auto       cpu = mgr->GetSrvAllocator()->GetIndexFromCPUHandle(
					cpuHandle);
				mgr->GetSrvAllocator()->Free(cpu);
			};

		ImGui_ImplDX12_Init(&initInfo);
	}

	void UImGuiManager::BeginFrame() {
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void UImGuiManager::EndFrame(ID3D12GraphicsCommandList* cmd) {
		ImGui::Render();

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd);

		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();

		ImGui::EndFrame();
	}

	void UImGuiManager::Shutdown() {
		ImGui::DestroyPlatformWindows();
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	DescriptorAllocator* UImGuiManager::GetSrvAllocator() {
		return mGraphicsDevice->GetSrvAllocator();
	}
}

#endif
