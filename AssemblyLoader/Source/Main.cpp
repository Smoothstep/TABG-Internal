#include <Windows.h>

#include "Utils.hpp"

auto WINAPI DllMain(HINSTANCE Instance, DWORD Reason, LPVOID Reserved) -> BOOL
{
	switch (Reason)
	{
		case DLL_PROCESS_ATTACH:
			LogInit();
		case DLL_PROCESS_DETACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
			break;
	}
	return TRUE;
}