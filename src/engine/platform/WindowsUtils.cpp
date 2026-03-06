#include <Windows.h>

#include <dwmapi.h>
#include <dxgi.h>
#include <intrin.h>
#include <lmcons.h>
#include <vector>

#include "WindowsUtils.h"

#include <wrl/client.h>

#pragma comment(lib, "Dwmapi.lib")

/// @brief Windowsのユーザー名を取得します
/// @return ユーザー名文字列
std::string WindowsUtils::GetWindowsUserName() {
	DWORD             bufferSize = UNLEN + 1; // +1 は null 文字分
	std::vector<char> buffer(bufferSize);
	if (GetUserNameA(buffer.data(), &bufferSize)) {
		std::string userName(buffer.data());
		// 空白をアンダースコアに置き換える
		for (char& ch : userName) {
			if (ch == ' ') {
				ch = '_';
			}
		}
		return userName;
	}
	return std::string("Windowsから取得できませんでした。");
}

/// @brief Windowsのコンピュータ名を取得します
/// @return コンピュータ名文字列
std::string WindowsUtils::GetWindowsComputerName() {
	DWORD             bufferSize = MAX_COMPUTERNAME_LENGTH + 1; // +1 は null 文字分
	std::vector<char> buffer(bufferSize);
	if (GetComputerNameA(buffer.data(), &bufferSize)) {
		return std::string(buffer.data());
	}
	return std::string("Windowsから取得できませんでした。");
}

/// @brief Windowsのバージョンを取得します
/// @return バージョン文字列
std::string WindowsUtils::GetWindowsVersion() {
	// RtlGetVersion を動的取得して、実際の Windows バージョンを取得する
	using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);

	const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
	if (!ntdll) {
		return "Windows";
	}

	const auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
		GetProcAddress(ntdll, "RtlGetVersion")
	);
	if (!rtlGetVersion) {
		return "Windows";
	}

	RTL_OSVERSIONINFOW osvi{};
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	if (rtlGetVersion(&osvi) != 0) {
		return "Windows";
	}

	// Windows 11 は major=10, minor=0 で、build >= 22000 で判定するのが一般的らしい
	const bool isWindows11 = osvi.dwMajorVersion == 10 && osvi.dwMinorVersion
	                         == 0 &&
	                         osvi.dwBuildNumber >= 22000;

	std::string name = isWindows11 ? "Windows 11" : "Windows";

	// 参考情報として build も付与
	name += " (Build " + std::to_string(
		osvi.dwBuildNumber
	) + ")";
	return name;
}

/// @brief CPUの名前を取得します
/// @return CPU名文字列
std::string WindowsUtils::GetCPUName() {
	int  cpuInfo[4] = {-1};
	char cpuBrand[0x40];
	__cpuid(cpuInfo, 0x80000002);
	memcpy(cpuBrand, cpuInfo, sizeof(cpuInfo));
	__cpuid(cpuInfo, 0x80000003);
	memcpy(cpuBrand + 16, cpuInfo, sizeof(cpuInfo));
	__cpuid(cpuInfo, 0x80000004);
	memcpy(cpuBrand + 32, cpuInfo, sizeof(cpuInfo));
	cpuBrand[48] = '\0';
	return std::string(cpuBrand);
}

/// @brief GPUの名前を取得します
/// @return GPU名文字列
std::string WindowsUtils::GetGPUName() {
	Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
	if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
		return "Failed to create DXGI Factory";
	}

	Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
	if (FAILED(factory->EnumAdapters1(0, &adapter))) {
		return "Failed to enumerate adapters";
	}

	DXGI_ADAPTER_DESC1 desc;
	adapter->GetDesc1(&desc);

	// ワイド文字列をマルチバイト文字列に変換
	const int sizeNeeded = WideCharToMultiByte(
		CP_UTF8, 0, desc.Description, -1, nullptr, 0, nullptr, nullptr
	);
	std::string gpuName(sizeNeeded - 1, 0); // 終端の null 文字を除くために -1
	WideCharToMultiByte(
		CP_UTF8, 0, desc.Description, -1, gpuName.data(),
		sizeNeeded, nullptr, nullptr
	);

	const int vRam = static_cast<int>(desc.DedicatedVideoMemory / (static_cast<
			                                  unsigned long long>(1024) *
		                                  1024));
	std::string ret = gpuName + " " + std::to_string(vRam) + " MB";
	return ret;
}

/// @brief 最大RAM容量を取得します
/// @return 最大RAM容量文字列
std::string WindowsUtils::GetRamMax() {
	MEMORYSTATUSEX memoryStatus;
	memoryStatus.dwLength = sizeof(memoryStatus);
	GlobalMemoryStatusEx(&memoryStatus);
	return std::to_string(memoryStatus.ullTotalPhys / 1024 / 1024) + "MB";
}

/// @brief 使用中のRAM容量を取得します
/// @return 使用中のRAM容量文字列
std::string WindowsUtils::GetRamUsage() {
	MEMORYSTATUSEX memoryStatus;
	memoryStatus.dwLength = sizeof(memoryStatus);
	GlobalMemoryStatusEx(&memoryStatus);
	return std::to_string(
		       (memoryStatus.ullTotalPhys - memoryStatus.ullAvailPhys) / 1024 /
		       1024
	       ) +
	       "MB";
}

/// @brief HRESULTコードに対応するメッセージを取得します
/// @param hr HRESULTコード
/// @return メッセージ文字列
std::string WindowsUtils::GetHresultMessage(const HRESULT hr) {
	char*        messageBuffer = nullptr;
	const size_t messageLength = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		hr,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPSTR>(&messageBuffer),
		0,
		nullptr
	);

	std::string message;
	if (messageLength > 0) {
		message = std::string(messageBuffer, messageLength);
	} else {
		message = "Unknown error code : " + std::to_string(hr);
	}

	// メモリ解放
	if (messageBuffer) {
		LocalFree(messageBuffer);
	}

	return message;
}

/// @brief レジストリからDWORD値を取得します
/// @param hKeyParent 親キーのハンドル
/// @param key キーのパス
/// @param name 値の名前
/// @param pData 取得したDWORD値の格納先ポインタ
/// @return 成功したらtrueを返す
bool WindowsUtils::RegistryGetDWord(
	void*       hKeyParent, const char* key,
	const char* name, unsigned long*    pData
) {
	DWORD len  = sizeof(DWORD);
	HKEY  hKey = nullptr;
	DWORD rc   = RegOpenKeyExA(
		static_cast<HKEY>(hKeyParent), key, 0, KEY_READ,
		&hKey
	);
	if (rc == ERROR_SUCCESS) {
		rc = RegQueryValueExA(
			hKey, name, nullptr, nullptr,
			reinterpret_cast<LPBYTE>(pData), &len
		);
	}
	RegCloseKey(hKey);
	return rc == ERROR_SUCCESS;
}

/// @brief アプリのテーマがダークモードかどうかを問い合わせます
/// @return ダークモードならtrueを返す
bool WindowsUtils::IsAppDarkTheme() {
	DWORD lightMode = 1;
	RegistryGetDWord(
		HKEY_CURRENT_USER,
		R"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)",
		"AppsUseLightTheme",
		&lightMode
	);
	return lightMode == 0;
}

/// @brief アプリのテーマがダークモードかどうかを問い合わせます
/// @return ダークモードならtrueを返す
bool WindowsUtils::IsSystemDarkTheme() {
	DWORD lightMode = 1;
	RegistryGetDWord(
		HKEY_CURRENT_USER,
		R"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)",
		"SystemUsesLightTheme",
		&lightMode
	);
	return lightMode == 0;
}

WindowsUtils::ProcessResult WindowsUtils::RunProcessCapture(
	const std::wstring& application,
	const std::wstring& arguments,
	const std::wstring& workingDirectory,
	unsigned long       timeoutMs
) {
	ProcessResult result{};

	SECURITY_ATTRIBUTES sa{};
	sa.nLength              = sizeof(sa);
	sa.bInheritHandle       = TRUE;
	sa.lpSecurityDescriptor = nullptr;

	HANDLE outRead = nullptr, outWrite = nullptr;
	if (!CreatePipe(&outRead, &outWrite, &sa, 0)) {
		result.win32Error = GetLastError();
		return result;
	}
	if (!SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0)) {
		result.win32Error = GetLastError();
		CloseHandle(outRead);
		CloseHandle(outWrite);
		return result;
	}

	STARTUPINFOW si{};
	si.cb        = sizeof(si);
	si.dwFlags   = STARTF_USESTDHANDLES;
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	// stderr も同じパイプにまとめる（簡易＆デッドロック回避）
	si.hStdOutput = outWrite;
	si.hStdError  = outWrite;

	PROCESS_INFORMATION pi{};

	// CreateProcessW はコマンドラインを書き換える可能性があるので可変バッファを渡す
	// applicationName は null にし、cmdLine にフルコマンドラインを渡す（パス探索に任せる）
	std::wstring cmdLine;
	if (!application.empty()) {
		cmdLine = L"\"" + application + L"\"";
		if (!arguments.empty()) {
			cmdLine += L" ";
			cmdLine += arguments;
		}
	} else {
		cmdLine = arguments;
	}
	std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
	cmdBuf.push_back(L'\0');

	const wchar_t* workDirPtr = workingDirectory.empty() ?
		                            nullptr :
		                            workingDirectory.c_str();

	BOOL ok = CreateProcessW(
		nullptr,
		cmdBuf.data(),
		nullptr,
		nullptr,
		TRUE,
		CREATE_NO_WINDOW,
		nullptr,
		workDirPtr,
		&si,
		&pi
	);

	// 親側で書き込み端を閉じる（EOF を受け取れるように）
	CloseHandle(outWrite);

	if (!ok) {
		result.win32Error = GetLastError();
		CloseHandle(outRead);
		return result;
	}

	auto readAll = [](const HANDLE h) -> std::string {
		std::string out;
		char        buf[4096];
		DWORD       read = 0;
		while (true) {
			const BOOL r = ReadFile(h, buf, sizeof(buf), &read, nullptr);
			if (!r || read == 0) {
				break;
			}
			out.append(buf, buf + read);
		}
		return out;
	};

	DWORD wait = WAIT_OBJECT_0;
	if (timeoutMs > 0) {
		wait = WaitForSingleObject(pi.hProcess, timeoutMs);
		if (wait == WAIT_TIMEOUT) {
			result.timedOut = true;
		}
	} else {
		WaitForSingleObject(pi.hProcess, INFINITE);
	}

	if (!result.timedOut) {
		// stderr も同じパイプにしているので全部 stdoutText に入れる
		result.stdoutText = readAll(outRead);
	}

	GetExitCodeProcess(pi.hProcess, &result.exitCode);

	CloseHandle(outRead);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	return result;
}

WindowsUtils::ProcessResult WindowsUtils::RunCmdCapture(
	const std::wstring& cmd,
	const std::wstring& workingDirectory,
	const unsigned long timeoutMs
) {
	// /S は "cmd" の引用符処理を正しくするため
	// /C は実行後に終了
	const std::wstring args = L"/S /C \"" + cmd + L"\"";
	return RunProcessCapture(L"cmd.exe", args, workingDirectory, timeoutMs);
}

WindowsUtils::ProcessResult WindowsUtils::RunPowerShellFileCapture(
	const std::wstring& scriptPath,
	const std::wstring& arguments,
	const std::wstring& workingDirectory,
	const unsigned long timeoutMs
) {
	// 実運用で詰まりやすい ExecutionPolicy を Bypass にし、プロファイル読み込みも切る
	std::wstring args =
		L"-NoProfile -ExecutionPolicy Bypass -File \"" + scriptPath + L"\"";
	if (!arguments.empty()) {
		args += L" ";
		args += arguments;
	}
	return RunProcessCapture(
		L"powershell.exe", args, workingDirectory, timeoutMs
	);
}

static std::string WideToUtf8(const std::wstring_view w) {
	if (w.empty()) {
		return {};
	}
	const int len = WideCharToMultiByte(
		CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr,
		nullptr
	);
	std::string out(len, '\0');
	WideCharToMultiByte(
		CP_UTF8, 0, w.data(), static_cast<int>(w.size()), out.data(), len,
		nullptr, nullptr
	);
	return out;
}

bool WindowsUtils::StartProcessCapture(
	const std::wstring& application,
	const std::wstring& arguments,
	ProcessHandle&      outHandle,
	const std::wstring& workingDirectory
) {
	CloseProcessHandle(outHandle);

	SECURITY_ATTRIBUTES sa{};
	sa.nLength        = sizeof(sa);
	sa.bInheritHandle = TRUE;

	HANDLE readPipe  = nullptr;
	HANDLE writePipe = nullptr;
	if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
		return false;
	}
	if (!SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0)) {
		CloseHandle(readPipe);
		CloseHandle(writePipe);
		return false;
	}

	STARTUPINFOW si{};
	si.cb         = sizeof(si);
	si.dwFlags    = STARTF_USESTDHANDLES;
	si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
	si.hStdOutput = writePipe;
	si.hStdError  = writePipe;

	PROCESS_INFORMATION pi{};

	std::wstring cmdLine = L"\"" + application + L"\"";
	if (!arguments.empty()) {
		cmdLine += L" ";
		cmdLine += arguments;
	}
	std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
	cmdBuf.push_back(L'\0');

	const wchar_t* workDirPtr = workingDirectory.empty() ?
		                            nullptr :
		                            workingDirectory.c_str();

	const BOOL ok = CreateProcessW(
		nullptr,
		cmdBuf.data(),
		nullptr,
		nullptr,
		TRUE,
		CREATE_NO_WINDOW,
		nullptr,
		workDirPtr,
		&si,
		&pi
	);

	CloseHandle(writePipe); // 親は書き込み端を閉じる
	if (!ok) {
		CloseHandle(readPipe);
		return false;
	}

	outHandle.hProcess  = pi.hProcess;
	outHandle.hThread   = pi.hThread;
	outHandle.hReadPipe = readPipe;
	return true;
}

bool WindowsUtils::PollProcessOutput(
	const ProcessHandle& handle, std::string& outChunk
) {
	outChunk.clear();
	if (!handle.hReadPipe) {
		return false;
	}

	DWORD avail = 0;
	if (!PeekNamedPipe(
		handle.hReadPipe, nullptr, 0, nullptr, &avail, nullptr
	)) {
		return false;
	}
	if (avail == 0) {
		return false;
	}

	std::string buf;
	buf.resize(std::min<DWORD>(avail, 4096));
	DWORD read = 0;
	if (!ReadFile(
		    handle.hReadPipe, buf.data(), static_cast<DWORD>(buf.size()), &read,
		    nullptr
	    ) || read == 0) {
		return false;
	}
	buf.resize(read);
	outChunk = std::move(buf);
	return true;
}

bool WindowsUtils::TryGetProcessExitCode(
	const ProcessHandle& handle, unsigned long& exitCode
) {
	if (!handle.hProcess) {
		return false;
	}
	DWORD code = STILL_ACTIVE;
	if (!GetExitCodeProcess(handle.hProcess, &code)) {
		return false;
	}
	if (code == STILL_ACTIVE) {
		return false;
	}
	exitCode = code;
	return true;
}

void WindowsUtils::CloseProcessHandle(ProcessHandle& handle) {
	if (handle.hReadPipe) {
		CloseHandle(handle.hReadPipe);
	}
	if (handle.hThread) {
		CloseHandle(handle.hThread);
	}
	if (handle.hProcess) {
		CloseHandle(handle.hProcess);
	}
	handle = {};
}

std::string WindowsUtils::ConvertCP932ToUtf8(const std::string& s) {
	if (s.empty()) {
		return {};
	}
	// CP932 -> UTF-16
	const int wlen = MultiByteToWideChar(
		932 /*CP932*/, 0, s.data(), static_cast<int>(s.size()), nullptr, 0
	);
	std::wstring w(wlen, L'\0');
	MultiByteToWideChar(
		932, 0, s.data(), static_cast<int>(s.size()), w.data(), wlen
	);
	// UTF-16 -> UTF-8
	return WideToUtf8(w);
}
