#include <Windows.h>
#include <stdio.h>

#include "Utils.hpp"

auto LogInit() -> bool
{
	AllocConsole();
	
	FILE* Strm;
	if (freopen_s(&Strm, "CONOUT$", "w", stdout))
	{
		return false;
	}

	if (freopen_s(&Strm, "CONOUT$", "w", stderr))
	{
		return false;
	}

	return true;
}