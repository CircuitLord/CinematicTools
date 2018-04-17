#pragma once
#include "Camera/CameraManager.h"
#include "UI.h"

#include "inih/cpp/INIReader.h"
#include <memory>
#include <Windows.h>

class Main
{
public:
  Main();
  ~Main();

  bool Initialize();
  void Run();

  CameraManager* GetCameraManager() { return m_pCameraManager.get(); }
  UI* GetUI() { return m_pUI.get(); }

private:
  void LoadConfig();
  void SaveConfig();

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
  std::unique_ptr<INIReader> m_pConfig;

  std::unique_ptr<CameraManager> m_pCameraManager;
  std::unique_ptr<UI> m_pUI;

public:
  Main(Main const&) = delete;
  void operator=(Main const&) = delete;
};

extern bool g_shutdown;

extern Main* g_mainHandle;
extern HINSTANCE g_dllHandle;

extern HINSTANCE g_gameHandle;
extern HWND g_gameHwnd;
extern WNDPROC g_origWndProc;
