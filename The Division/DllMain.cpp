#include <stdio.h>
#include <Windows.h>

#include "Main.h"

Main* g_mainHandle = nullptr;
HINSTANCE g_dllHandle = NULL;
bool g_shutdown = false;

DWORD WINAPI Initialize(LPVOID arg)
{
  HINSTANCE* pDllHandle = static_cast<HINSTANCE*>(arg);
  g_dllHandle = *pDllHandle;

  g_mainHandle = new Main();
  g_mainHandle->Initialize();

  g_mainHandle->Release();
  delete g_mainHandle;

  return 1;
}

DWORD WINAPI DllMain(_In_ HINSTANCE _DllHandle, _In_ unsigned long _Reason, _In_opt_ void* _Reserved)
{
  if (_Reason == DLL_PROCESS_ATTACH)
    CreateThread(0, 0, &Initialize, new HINSTANCE(_DllHandle), 0, 0);

  return 1;
}