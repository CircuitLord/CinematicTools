#include "Main.h"
#include "Util/Util.h"
#include "Northlight.h"

#include <boost/filesystem.hpp>
#include <boost/chrono.hpp>
#include <fstream>

static const char* g_gameName = "Quantum Break";
static const char* g_moduleName = "QuantumBreak.exe";
static const char* g_className = "AppWindowClass";
static const char* g_windowTitle = "Quantum Break - 1.0.126.0307";
static const char* g_configFile = "./Cinematic Tools/config.ini";

Main* g_mainHandle = nullptr;
HINSTANCE g_dllHandle = NULL;
HINSTANCE g_gameHandle = NULL;
HWND g_gameHwnd = NULL;
WNDPROC g_origWndProc = 0;
bool g_shutdown = false;
bool g_hasFocus = false;

ID3D11DeviceContext* g_d3d11Context = nullptr;
ID3D11Device* g_d3d11Device = nullptr;
IDXGISwapChain* g_dxgiSwapChain = nullptr;

HMODULE g_aiModule = NULL;
HMODULE g_d3dModule = NULL;
HMODULE g_rendererModule = NULL;
HMODULE g_rlModule = NULL;
HMODULE g_qbModule = NULL;

Main::Main()
{

}

Main::~Main()
{
}

bool Main::Initialize()
{
  boost::filesystem::path dir("Cinematic Tools");
  if (!boost::filesystem::exists(dir))
    boost::filesystem::create_directory(dir);

  util::log::Init();
  util::log::Write("Cinematic Tools for %s\n", g_gameName);

  // Needed for ImGui + other functionality
  g_gameHwnd = FindWindowA(g_className, NULL);
  if (g_gameHwnd == NULL)
  {
    g_gameHwnd = FindWindowA(NULL, g_windowTitle);
    if (g_gameHwnd == NULL)
    {
      util::log::Error("Failed to retrieve window handle, GetLastError 0x%X", GetLastError());
      return false;
    }
  }

  // Used for relative offsets
  g_gameHandle = GetModuleHandleA(g_moduleName);
  g_qbModule = g_gameHandle;
  if (g_gameHandle == NULL)
  {
    util::log::Error("Failed to retrieve module handle, GetLastError 0x%X", GetLastError());
    return false;
  }

  g_aiModule = GetModuleHandleA("ai_x64_f");
  g_d3dModule = GetModuleHandleA("d3d_x64_f");
  g_rendererModule = GetModuleHandleA("renderer_x64_f");
  g_rlModule = GetModuleHandleA("rl_x64_f");

  g_d3d11Device = Northlight::d3d::Device::GetDevice(); // Fetch ID3D11Device
  g_d3d11Context = Northlight::d3d::Device::GetContext(); // Fetch context
  g_dxgiSwapChain = Northlight::d3d::Device::Singleton()->m_pSwapchain; // Fetch SwapChain

  if (!g_d3d11Context || !g_d3d11Device || !g_dxgiSwapChain)
  {
    util::log::Error("Failed to retrieve Dx11 interfaces");
    util::log::Error("Device 0x%I64X DeviceContext 0x%I64X SwapChain 0x%I64X", g_d3d11Device, g_d3d11Context, g_dxgiSwapChain);
    return false;
  }

  // Retrieve game version and make a const variable for whatever version
  // the tools support. If versions mismatch, scan for offsets.
  // util::offsets::Scan();

  m_pCameraManager = std::make_unique<CameraManager>();
  m_pInputSystem = std::make_unique<InputSystem>();
  m_pUI = std::make_unique<UI>();

  m_pInputSystem->Initialize();
  if (!m_pUI->Initialize())
    return false;

  util::hooks::Init();
  LoadConfig();

  // Subclass the window with a new WndProc to catch messages
  g_origWndProc = (WNDPROC)SetWindowLongPtr(g_gameHwnd, -4, (LONG_PTR)&WndProc);
  if (g_origWndProc == 0)
  {
    util::log::Error("Failed to set WndProc, GetLastError 0x%X", GetLastError());
    return false;
  }

  return true;
}

void Main::Run()
{
  // Main update loop
  boost::chrono::high_resolution_clock::time_point lastUpdate = boost::chrono::high_resolution_clock::now();
  while (!g_shutdown)
  {
    boost::chrono::duration<double> dt = boost::chrono::high_resolution_clock::now() - lastUpdate;
    lastUpdate = boost::chrono::high_resolution_clock::now();

    m_pCameraManager->Update(dt.count());
    m_pUI->Update(dt.count());

    // Check if config has been affected, if so, save it
    m_dtConfigCheck += dt.count();
    if (m_dtConfigCheck > 10.f)
    {
      m_dtConfigCheck = 0;
      if (m_ConfigChanged)
      {
        m_ConfigChanged = false;
        SaveConfig();
      }
    }

    Sleep(10);
  }

  // Save config and disable hooks before exit
  SaveConfig();
  util::hooks::SetHookState(false);
}

void Main::LoadConfig()
{
  // Read config.ini using inih by Ben Hoyt
  // https://github.com/benhoyt/inih

  m_pConfig = std::make_unique<INIReader>(g_configFile);
  int parseResult = m_pConfig->ParseError();

  // If there's problems reading the file, notify the user.
  // Code-wise it should be safe to just continue,
  // since you can still request variables from INIReader.
  // They'll just return the specified default value.
  if (parseResult != 0)
    util::log::Warning("Config file could not be loaded, using default settings");

  m_pCameraManager->ReadConfig(m_pConfig.get());
  m_pInputSystem->ReadConfig(m_pConfig.get());
}

void Main::SaveConfig()
{
  std::fstream file;
  file.open(g_configFile, std::ios_base::out | std::ios_base::trunc);

  if (!file.is_open())
  {
    util::log::Error("Could not save config, failed to open file for writing. GetLastError 0x%X", GetLastError());
    return;
  }

  file << m_pCameraManager->GetConfig();
  file << m_pInputSystem->GetConfig();
  
  file.close();
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK Main::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
    return TRUE;

  switch (uMsg)
  {
  case WM_ACTIVATE:
  {
    // Focus event
    g_hasFocus = (wParam != WA_INACTIVE);
    break;
  }
  case WM_KEYDOWN:
  {
    if (g_mainHandle->m_pInputSystem->HandleKeyMsg(wParam, lParam))
      return TRUE;

    break;
  }
  case WM_MOUSEMOVE: case WM_MOUSEWHEEL:
  {
    g_mainHandle->m_pInputSystem->HandleMouseMsg(uMsg, wParam, lParam);

    if (g_mainHandle->m_pUI->IsEnabled())
      return TRUE;

    break;
  }
  case WM_INPUT:
  {
    // To prevent UI toggle from rotating the camera
    if (!g_mainHandle->m_pUI->IsEnabled())
      g_mainHandle->m_pInputSystem->HandleRawInput(lParam);

    if ((g_mainHandle->m_pCameraManager->IsCameraEnabled()
          && g_mainHandle->m_pCameraManager->IsKbmDisabled()
          && !Northlight::r::IsPaused())
      || g_mainHandle->m_pUI->IsEnabled())
    {
      return TRUE;
    }
    break;
  }
  case WM_SIZE:
    // Resize event
    g_mainHandle->m_pUI->OnResize();
    break;
  case WM_EXITSIZEMOVE:
    util::log::Write("WM_EXITSIZEMOVE");
    Northlight::rend::UpdateResolution();
    break;
  case WM_DESTROY:
    g_shutdown = true;
    break;
  }

  return CallWindowProc(g_origWndProc, hwnd, uMsg, wParam, lParam);
}

