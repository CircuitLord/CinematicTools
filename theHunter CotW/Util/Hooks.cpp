#include "Util.h"
#include "../Main.h"

#include <MinHook.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <unordered_map>
#include <Windows.h>

#pragma comment(lib, "libMinHook.x64.lib")

using namespace DirectX;

// Function definitions
typedef DWORD(WINAPI* tIDXGISwapChain_Present)(IDXGISwapChain*, UINT, UINT);
typedef __int64(__fastcall* tCameraUpdate)(__int64, float, float);

//////////////////////////
////   RENDER HOOKS   ////
//////////////////////////

// Most games today run on DirectX 11. This hook is called just before
// the image is actually drawn on screen so we can draw more stuff
// on top, like the tools UI.

tIDXGISwapChain_Present oIDXGISwapChain_Present = nullptr;
DWORD WINAPI hIDXGISwapChain_Present(IDXGISwapChain* pSwapchain, UINT SyncInterval, UINT Flags)
{
  if (!g_shutdown)
    g_mainHandle->GetUI()->Draw();

  return oIDXGISwapChain_Present(pSwapchain, SyncInterval, Flags);
}

//////////////////////////
////   CAMERA HOOKS   ////
//////////////////////////

// This is an example of what a camera hook could look like.
// Different games and engines have different kinds of methods
// and it's up to you to figure out what to hook and how to
// override the game's camera. Some games might require multiple
// hooks.

tCameraUpdate oCameraUpdate = nullptr;
__int64 __fastcall hCameraUpdate(__int64 a1, float a2, float a3)
{
  if (g_mainHandle->GetCameraManager()->IsCameraEnabled())
  {
    XMFLOAT4X4* pMatrix1 = reinterpret_cast<XMFLOAT4X4*>(a1);
    XMFLOAT4X4* pMatrix2 = reinterpret_cast<XMFLOAT4X4*>(a1 + 0x2B0);
    XMFLOAT4X4* pMatrix3 = reinterpret_cast<XMFLOAT4X4*>(a1 + 0x2F0);
    XMFLOAT4X4* pMatrix4 = reinterpret_cast<XMFLOAT4X4*>(a1 + 0x3C0);
    XMFLOAT4X4* pMatrix5 = reinterpret_cast<XMFLOAT4X4*>(a1 + 0x400);
    XMFLOAT4X4* pMatrix6 = reinterpret_cast<XMFLOAT4X4*>(a1 + 0x3C0);
    XMFLOAT4X4* pMatrix7 = reinterpret_cast<XMFLOAT4X4*>(a1 + 0x400);
    XMFLOAT4X4* pMatrix8 = reinterpret_cast<XMFLOAT4X4*>(a1 + 0x4D0);
    XMFLOAT4X4* pMatrix9 = reinterpret_cast<XMFLOAT4X4*>(a1 + 0x510);

    XMFLOAT4X4& myTransform = g_mainHandle->GetCameraManager()->GetCameraTrans();

    //*pMatrix1 = myTransform;
    //*pMatrix2 = myTransform;
    //*pMatrix3 = myTransform;
    //*pMatrix4 = myTransform;
    //*pMatrix5 = myTransform;
    //*pMatrix6 = myTransform;
    //*pMatrix7 = myTransform;
    *pMatrix8 = myTransform;
    *pMatrix9 = myTransform;
    return 0;
  }
  else
  {
    XMFLOAT4X4* pMatrix1 = reinterpret_cast<XMFLOAT4X4*>(a1);
    g_mainHandle->GetCameraManager()->SetResetMatrix(*pMatrix1);
  }

  return oCameraUpdate(a1, a2, a3);
}
 

//////////////////////////
////   INPUT HOOKS    ////
//////////////////////////

// I like to try and provide a way to still move your character
// while having the camera enabled. This usually works by finding
// a function that specifically controls your character's input.
// Useful when you want to record a video and be your own actor.
// CameraManager provides IsKbmDisabled() and IsGamepadDisabled()

//////////////////////////
////   OTHER HOOKS    ////
//////////////////////////

// These could be hooks on AI to disable them, object iterators
// to find certain components, something to disable the usual
// outlines on friendly players/characters etc...
// Hooks on cursor functions are usually needed to show the mouse
// when tools UI is open.


/*----------------------------------------------------------------*/

namespace
{
  std::unordered_map<std::string, util::hooks::Hook> m_CreatedHooks;
}

// Creates a normal function hook with MinHook, 
// which places a jmp instruction at the start of the function.
template <typename T>
static void CreateHook(std::string const& name, __int64 target, PVOID hook, T original)
{
  LPVOID* pOriginal = reinterpret_cast<LPVOID*>(original);
  MH_STATUS result = MH_CreateHook((LPVOID)target, hook, pOriginal);
  if (result != MH_OK)
  {
    util::log::Error("Could not create %s hook. MH_STATUS 0x%X error code 0x%X", name, result, GetLastError());
    return;
  }

  result = MH_EnableHook((LPVOID)target);
  if (result != MH_OK)
  {
    util::log::Error("Could not enable %s hook. MH_STATUS 0x%X error code 0x%X", name, result, GetLastError());
    return;
  }

  util::hooks::Hook hookInfo{ 0 };
  hookInfo.Address = target;
  hookInfo.Type = util::hooks::HookType::MinHook;

  m_CreatedHooks.emplace(name, hookInfo);
}

// Write to VTable
static PBYTE WINAPI WriteToVTable(PDWORD64* ppVTable, PVOID hook, SIZE_T iIndex)
{
  DWORD dwOld = 0;
  VirtualProtect((void*)((*ppVTable) + iIndex), sizeof(PDWORD64), PAGE_EXECUTE_READWRITE, &dwOld);

  PBYTE pOrig = ((PBYTE)(*ppVTable)[iIndex]);
  (*ppVTable)[iIndex] = (DWORD64)hook;

  VirtualProtect((void*)((*ppVTable) + iIndex), sizeof(PDWORD64), dwOld, &dwOld);
  return pOrig;
}

// Hooks a function by changing the address at given index
// in the virtual function table.
template <typename T>
static void CreateVTableHook(std::string const& name, PDWORD64* ppVTable, PVOID hook, SIZE_T iIndex, T original)
{
  LPVOID* pOriginal = reinterpret_cast<LPVOID*>(original);
  *pOriginal = reinterpret_cast<LPVOID>(WriteToVTable(ppVTable, hook, iIndex));

  util::hooks::Hook hookInfo{ 0 };
  hookInfo.Address = (__int64)ppVTable;
  hookInfo.Index = iIndex;
  hookInfo.Type = util::hooks::HookType::VTable;
  hookInfo.Original = pOriginal;

  m_CreatedHooks.emplace(name, hookInfo);
}

void util::hooks::Init()
{
  util::log::Write("Initializing hooks");
  MH_STATUS status = MH_Initialize();
  if (status != MH_OK)
    util::log::Error("Failed to initialize MinHook, MH_STATUS 0x%X", status);

  CreateHook("CameraUpdate", util::offsets::GetOffset("OFFSET_CAMERAUPDATE"), hCameraUpdate, &oCameraUpdate);
  CreateVTableHook("SwapChainPresent", (PDWORD64*)g_dxgiSwapChain, hIDXGISwapChain_Present, 8, &oIDXGISwapChain_Present);

  util::log::Ok("Hooks initialized");
}

// In some cases it's useful or even required to disable all hooks or just certain ones
void util::hooks::SetHookState(bool enable, std::string const& name)
{
  if (name.empty())
  {
    MH_STATUS status = enable ? MH_EnableHook(MH_ALL_HOOKS) : MH_DisableHook(MH_ALL_HOOKS);
    if (status != MH_OK)
      util::log::Error("MinHook failed to %s all hooks, MH_STATUS 0x%X", (enable ? "enable" : "disable"), status);

    for (auto& entry : m_CreatedHooks)
    {
      Hook& hook = entry.second;
      if (hook.Type == HookType::MinHook) continue;
      if (hook.Enabled != enable)
      {
        *hook.Original = WriteToVTable((PDWORD64*)hook.Address, *hook.Original, hook.Index);
        hook.Enabled = enable;
      }
      else
        util::log::Warning("VTable hook %s is already %s", name.c_str(), enable ? "enabled" : "disabled");
    }
  }
  else
  {
    auto result = m_CreatedHooks.find(name);
    if (result == m_CreatedHooks.end())
    {
      util::log::Error("Hook %s does not exit", name.c_str());
      return;
    }

    Hook& hook = result->second;
    if (hook.Type == HookType::MinHook)
    {
      MH_STATUS status = enable ? MH_EnableHook((LPVOID)hook.Address) : MH_DisableHook((LPVOID)hook.Address);
      if (status != MH_OK)
        util::log::Error("MinHook failed to %s hook %s, MH_STATUS 0x%X", (enable ? "enable" : "disable"), name.c_str(), status);
    }
    else
    {
      if (hook.Enabled != enable)
      {
        *hook.Original = WriteToVTable((PDWORD64*)hook.Address, *hook.Original, hook.Index);
        hook.Enabled = enable;
      }
      else
        util::log::Warning("VTable hook %s is already %s", name.c_str(), enable ? "enabled" : "disabled");
    }
  }
}
