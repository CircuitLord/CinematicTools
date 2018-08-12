#include <Windows.h>
#include <fstream>
#include "Main.h"

BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		AllocConsole();
		freopen("CONOUT$", "w", stdout);
		freopen("CONIN$", "r", stdin);
		Sleep(1000);
		HANDLE thread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)Main::Init, NULL, NULL, NULL);
		CloseHandle(thread);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
	}

	return true;
}