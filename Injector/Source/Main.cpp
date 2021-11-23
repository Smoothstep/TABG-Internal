#include <Windows.h>
#include <iostream>
#include <tchar.h>
#include <ostream>
#include <fstream>
#include <Psapi.h>
#include <DbgHelp.h>
#include <array>
#include <ranges>
#include <filesystem>
#include <string_view>
#include <tuple>

using namespace std::string_view_literals;

#include "Utils.hpp"
#include "Messages.hpp"

#pragma comment(lib, "psapi")
#pragma comment(lib, "kernel32")

auto main(int Argc, char* Argv[]) -> int
{
	auto Ret = [](int Value)
	{
		return std::cin.get();
	};

	if (Argc < 4)
	{
		Log("Usage: Injector <WindowName> <PathToAssembly> <MethodToCall>");
		return std::cin.get();
	}

	std::filesystem::path Path{ Argv[2] };

	if (!std::filesystem::exists(Path))
	{
		Err("Path invalid: ", Path);
		return std::cin.get();
	}

	HMODULE Lib = LoadLibraryA("AssemblyLoader.dll");
	if (!Lib)
	{
		Err("Failed to load lib: Loader.dll. Error code: ", GetLastError());
		return std::cin.get();
	}

	HWND GameWindow = FindWindowA(0, Argv[1]);
	if (!GameWindow)
	{
		Err("Failed to find window: ", Argv[1]);
		return std::cin.get();
	}

	DWORD ProcessID = 0;
	DWORD ThreadID = GetWindowThreadProcessId(GameWindow, &ProcessID);
	if (!ThreadID)
	{
		Err("Failed to obtain main thread id for window: ", GameWindow);
		return std::cin.get();
	}

	HOOKPROC Proc = reinterpret_cast<HOOKPROC>(GetProcAddress(Lib, "?InjAsm@@YA_JH_K_J@Z"));
	if (!Proc) 
	{
		Err("Failed to obtain main thread id for window: ", GameWindow);
		return std::cin.get();
	}

	SECURITY_ATTRIBUTES SecAttr{};

	constexpr DWORD InfoSz = sizeof(STargetAssembly);

	ScopedHandle<&CreateFileMapping, &CloseHandle> Mapping{ INVALID_HANDLE_VALUE, &SecAttr, PAGE_READWRITE, 0, InfoSz, "TargetAssembly" };
	if (!Mapping)
	{
		Err("Failed to create file mapping");
		return std::cin.get();
	}

	STargetAssembly* MappingMem = reinterpret_cast<STargetAssembly*>(MapViewOfFile(Mapping, FILE_MAP_WRITE, 0, 0, InfoSz));
	if (!MappingMem)
	{
		Err("Failed to map view of file");
		return std::cin.get();
	}

	strcpy_s(MappingMem->Method , Argv[3]);
	strcpy_s(MappingMem->ModulePath, std::filesystem::absolute(Path).string().c_str());

	HHOOK Hook = SetWindowsHookExW(WH_GETMESSAGE, Proc, Lib, ThreadID);
	if (!Hook)
	{
		Err("Failed to place hook. Err: ", GetLastError());
		return std::cin.get();
	}

	if (!PostMessage(GameWindow, RequestLoad, 0, 0))
	{
		Err("Failed to place hook. Err: ", GetLastError());
		return std::cin.get();
	}

	Sleep(1000);

	if (!UnhookWindowsHookEx(Hook))
	{
		Log("Unhook failed.");
	}

	Log("Hook placed.");

	return std::cin.get();
}