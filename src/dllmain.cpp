#include "dllmain.hpp"

#include "amethyst/runtime/HookManager.hpp"
#include "amethyst/Log.hpp"
#include "amethyst-deps/safetyhook.hpp"
#include <windows.h>
#include <string>

// -----------------------------------------------------------------------------
// Typedefs
// -----------------------------------------------------------------------------
using CreateFileW_t = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using CreateFileA_t = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using CreateFile2_t = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, DWORD, LPCREATEFILE2_EXTENDED_PARAMETERS);
using CreateFileTransactedW_t = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE, HANDLE);

// -----------------------------------------------------------------------------
// Trampolines
// -----------------------------------------------------------------------------
static SafetyHookInline g_createFileWTrampoline;
static SafetyHookInline g_createFileATrampoline;
static SafetyHookInline g_createFile2Trampoline;
static SafetyHookInline g_createFileTxTrampoline;

// -----------------------------------------------------------------------------
// Recursion guard
// -----------------------------------------------------------------------------
thread_local int g_hook_depth = 0;
struct HookGuard { HookGuard() { ++g_hook_depth; } ~HookGuard() { --g_hook_depth; } };

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
static std::string WideToUtf8(LPCWSTR w)
{
	if (!w) return {};
	int size = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
	if (size <= 0) return {};
	std::string s(size, '\0');
	WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), size, nullptr, nullptr);
	if (!s.empty() && s.back() == '\0') s.pop_back();
	return s;
}

static std::string AnsiToUtf8(LPCSTR s)
{
	if (!s) return {};
	int wlen = MultiByteToWideChar(CP_ACP, 0, s, -1, nullptr, 0);
	if (wlen <= 0) return {};
	std::wstring w(static_cast<size_t>(wlen), L'\0');
	MultiByteToWideChar(CP_ACP, 0, s, -1, w.data(), wlen);
	if (!w.empty() && w.back() == L'\0') w.pop_back();
	return WideToUtf8(w.c_str());
}

std::wstring ConvertToWString(const std::string& str) {
	return std::wstring(str.begin(), str.end());
}
bool StartsWith(LPCWSTR str, LPCWSTR prefix) {
	if (!str || !prefix) return false;
	size_t prefixLen = wcslen(prefix);
	return wcsncmp(str, prefix, prefixLen) == 0;
}

std::string ConvertToString(LPCWSTR wstr) {
	return std::string(wstr, wstr + wcslen(wstr));
}

const wchar_t* getExecutablePathW() {

	wchar_t path[MAX_PATH]; // buffer for ANSI string

	// hModule = NULL → current executable
	DWORD result = GetModuleFileNameW(NULL, path, MAX_PATH);


	if (result == 0) {
		Log::Info("Error getting executable path: {}", GetLastError());
		return NULL;
	}

	// Remove the filename, leaving just the folder
	//PathRemoveFileSpecW(path);
	std::wstring fullPath(path);
	size_t pos = fullPath.find_last_of(L"\\/");
	std::wstring folder = (pos != std::wstring::npos) ? fullPath.substr(0, pos) : fullPath;
	return folder.c_str();
}

std::string normalizeSeparators(std::string path) {
	for (char& c : path) {
		if (c == '\\') c = '/';
	}
	return path;
}


const wchar_t* getRendererPathW() {
	fs::path folderPath = fs::path(getExecutablePathW());
	folderPath.append("data");
	folderPath.append("renderer");

	return folderPath.c_str();
}
static LPCWSTR handleReroute(LPCWSTR input) {
	auto normalizedInput = normalizeSeparators(ConvertToString(input));
	auto prefix = normalizeSeparators(shaderManager->mSourcePath);

	if (normalizedInput.starts_with(prefix)) {
		if (shaderManager->handleLoadFile(normalizedInput)) {
			//Log::Info("CreateFileW Rerouted: {} into {}", ConvertToString(input), normalizedInput);
			auto result = ConvertToWString(normalizedInput);
			auto resultCString = result.c_str();
			return resultCString;
		}
	}
	return input;
}

// -----------------------------------------------------------------------------
// Detours
// -----------------------------------------------------------------------------
extern "C" HANDLE WINAPI Detour_CreateFileW(
	LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
	DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	if (g_hook_depth == 0) {
		HookGuard guard;
		std::string path = WideToUtf8(lpFileName);
	}

	return g_createFileWTrampoline.call<HANDLE, LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE>(
		handleReroute(lpFileName), dwDesiredAccess, dwShareMode, lpSecurityAttributes,
		dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

extern "C" HANDLE WINAPI Detour_CreateFileA(
	LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
	DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	if (g_hook_depth == 0) {
		HookGuard guard;
		std::string path = AnsiToUtf8(lpFileName);
		Log::Info("CreateFileA: {}", path.empty() ? "(null)" : path.c_str());
	}

	return g_createFileATrampoline.call<HANDLE, LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE>(
		lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
		dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

extern "C" HANDLE WINAPI Detour_CreateFile2(
	LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
	DWORD dwCreationDisposition, LPCREATEFILE2_EXTENDED_PARAMETERS pExtendedParameters)
{
	if (g_hook_depth == 0) {
		HookGuard guard;
		std::string path = WideToUtf8(lpFileName);
		Log::Info("CreateFile2: {}", path.empty() ? "(null)" : path.c_str());
	}

	return g_createFile2Trampoline.call<HANDLE, LPCWSTR, DWORD, DWORD, DWORD, LPCREATEFILE2_EXTENDED_PARAMETERS>(
		lpFileName, dwDesiredAccess, dwShareMode, dwCreationDisposition, pExtendedParameters);
}

extern "C" HANDLE WINAPI Detour_CreateFileTransactedW(
	LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
	DWORD dwFlagsAndAttributes, HANDLE hTemplateFile, HANDLE hTransaction)
{
	if (g_hook_depth == 0) {
		HookGuard guard;
		std::string path = WideToUtf8(lpFileName);
		Log::Info("CreateFileTransactedW: {}", path.empty() ? "(null)" : path.c_str());
	}

	return g_createFileTxTrampoline.call<HANDLE, LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE, HANDLE>(
		lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
		dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile, hTransaction);
}

// -----------------------------------------------------------------------------
// Installer
// -----------------------------------------------------------------------------
void InstallWin32FileHooks(Amethyst::HookManager& hooks)
{
	HMODULE kernel32 = GetModuleHandleW(L"KernelBase.dll");
	if (!kernel32) kernel32 = LoadLibraryW(L"KernelBase.dll");
	if (!kernel32) {
		Log::Warning("InstallWin32FileHooks: failed to load KernelBase.dll");
		return;
	}

	auto hook = [&](const char* name, SafetyHookInline& tramp, void* detour) {
		FARPROC p = GetProcAddress(kernel32, name);
		if (!p) {
			Log::Warning("InstallWin32FileHooks: GetProcAddress({}) failed", name);
			return;
		}
		hooks.CreateHookAbsolute(tramp, reinterpret_cast<uintptr_t>(p), detour);
		Log::Info("Hooked {}", name);
		};

	hook("CreateFileW", g_createFileWTrampoline, reinterpret_cast<void*>(&Detour_CreateFileW));
	//hook("CreateFileA", g_createFileATrampoline, reinterpret_cast<void*>(&Detour_CreateFileA));
	//hook("CreateFile2", g_createFile2Trampoline, reinterpret_cast<void*>(&Detour_CreateFile2));
	//hook("CreateFileTransactedW", g_createFileTxTrampoline, reinterpret_cast<void*>(&Detour_CreateFileTransactedW));
}


// Subscribed to amethysts on start join game event in Initialize
void OnStartJoinGame(OnStartJoinGameEvent& event)
{
	Log::Info("OnStartJoinGame!");
	MinecraftGame* game = Amethyst::GetClientCtx().mClientInstance->mMinecraftGame;
	// Get the vptr (points to vtable)
	//void** vtable = *(void***)&game;
	// Print the vtable address itself
	//std::cout << "vtable address: " << vtable << "\n";
}

std::string WCharToString(const wchar_t* wstr) {
	return std::string(wstr, wstr + wcslen(wstr));
}
// Ran when the mod is loaded into the game by AmethystRuntime
ModFunction void Initialize(AmethystContext& ctx, const Amethyst::Mod& mod)
{
	// Initialize Amethyst mod backend
	Amethyst::InitializeAmethystMod(ctx, mod);

	// Add a listener to a built in amethyst event
	Amethyst::GetEventBus().AddListener<OnStartJoinGameEvent>(&OnStartJoinGame);
	Log::Info("Executable Path: {}", WCharToString(getRendererPathW()));
	auto folder = Amethyst::GetPlatform().GetAmethystFolder().append("shaders").generic_string();
	shaderManager = new ShaderManager(WCharToString(getRendererPathW()), folder);

	InstallWin32FileHooks(Amethyst::GetHookManager());

	Log::Info("Folder: {}", folder);
}
