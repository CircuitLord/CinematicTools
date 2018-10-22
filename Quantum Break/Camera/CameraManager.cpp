#include "CameraManager.h"
#include "../Main.h"
#include "../Util/Util.h"
#include "../Util/ImGuiEXT.h"
#include "../Northlight.h"
#include <Windows.h>

using namespace DirectX;

CameraManager::CameraManager() :
  m_CameraEnabled(false),
  m_FirstEnable(true),
  m_GamepadDisabled(true),
  m_KbmDisabled(true),
  m_TrackPlayer(),
  m_AutoReset(true),
  m_OverrideAspectRatio(false),
  m_AspectRatio(1.777777f)
{

}

CameraManager::~CameraManager()
{

}

void CameraManager::HotkeyUpdate()
{
  InputSystem* pInput = g_mainHandle->GetInputSystem();

  if (pInput->IsActionDown(Action::ToggleCamera))
  {
    ToggleCamera();
    while (pInput->IsActionDown(Action::ToggleCamera))
      Sleep(1);
  }

  if (pInput->IsActionDown(Action::ToggleHUD))
  {
    float* HUDVisibility = (float*)((__int64)g_gameHandle + 0x1174118);
    *HUDVisibility = (*HUDVisibility == 1.0f) ? 0 : 1.0f;

    while (pInput->IsActionDown(Action::ToggleHUD))
      Sleep(1);
  }

  if (pInput->IsActionDown(Action::ToggleFreezeTime))
  {
    m_TimeFreezeEnabled = !m_TimeFreezeEnabled;
    float* pFramerate = Northlight::GetFrameRate();

    *pFramerate = m_TimeFreezeEnabled ? 0 : 30.f;

    while (pInput->IsActionDown(Action::ToggleFreezeTime))
      Sleep(1);
  }

  if (m_CameraEnabled)
  {
    if (pInput->IsActionDown(Action::Track_CreateNode))
    {
      m_TrackPlayer.CreateNode(m_Camera);

      while (pInput->IsActionDown(Action::Track_CreateNode))
        Sleep(1);
    }

    if (pInput->IsActionDown(Action::Track_DeleteNode))
    {
      m_TrackPlayer.DeleteNode();

      while (pInput->IsActionDown(Action::Track_DeleteNode))
        Sleep(1);
    }

    if (pInput->IsActionDown(Action::Track_Play))
    {
      m_TrackPlayer.Toggle();

      while (pInput->IsActionDown(Action::Track_Play))
        Sleep(1);
    }
  }
}

void CameraManager::Update(double dt)
{
  if (!m_CameraEnabled) return;

  if (m_UiRequestReset)
  {
    m_UiRequestReset = false;
    ResetCamera();
  }

  UpdateInput(dt);
  UpdateCamera(dt);
}

void CameraManager::DrawUI()
{
  ImGuiIO& io = ImGui::GetIO();

  ImGui::Dummy(ImVec2(0, 10));
  ImGui::Dummy(ImVec2(300, 0));

  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 1));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
  ImGui::PushFont(io.Fonts->Fonts[3]);
  ImGui::SameLine();
  if (ImGui::ToggleButton(m_CameraEnabled ? "Disable" : "Enable", ImVec2(100, 25), m_CameraEnabled, true))
    ToggleCamera();
  ImGui::SameLine();
  m_UiRequestReset |= ImGui::Button("Reset", ImVec2(100, 25));

  ImGui::PopFont();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
  ImGui::PushFont(io.Fonts->Fonts[4]);

  ImGui::Dummy(ImVec2(0, 0));
  ImGui::Columns(4, 0, false);
  ImGui::NextColumn();
  ImGui::SetColumnOffset(-1, 12);

  ImGui::PushItemWidth(200);
  bool configChanged = false;

  ImGui::Text("Movement speed");
  configChanged |= ImGui::InputFloat("##CameraMovementSpeed", &m_Camera.MovementSpeed, 0.1f, 1.0f, 2);
  ImGui::Text("Rotation speed");
  configChanged |= ImGui::InputFloat("##CameraRotationSpeed", &m_Camera.RotationSpeed, 0.1f, 1.0f, 2);
  ImGui::Text("Roll speed");
  configChanged |= ImGui::InputFloat("##CameraRollSpeed", &m_Camera.RollSpeed, 0.1f, 1.0f, 2);
  ImGui::Text("FoV speed");
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 10));
  configChanged |= ImGui::InputFloat("##CameraFoVSpeed", &m_Camera.FovSpeed, 0.1f, 1.0f, 2);

  if (configChanged)
    g_mainHandle->OnConfigChanged();

  ImGui::Checkbox("Reset camera automatically", &m_AutoReset);
  ImGui::PopStyleVar();

  ImGui::NextColumn();
  ImGui::SetColumnOffset(-1, 290);
  ImGui::PushItemWidth(200);

  ImGui::Text("Field of view");
  ImGui::InputFloat("##CameraFoV", &m_Camera.FieldOfView, 1.f, 1.f, 2);

  ImGui::Text("Aspect ratio");
  ImGui::InputFloat("##CameraRatio", &m_AspectRatio);
  ImGui::Checkbox("Override aspect ratio", &m_OverrideAspectRatio);

  ImGui::NextColumn();
  ImGui::SetColumnOffset(-1, 552);
  ImGui::PushItemWidth(200);

  m_TrackPlayer.DrawUI();
  ImGui::PopFont();
}

void CameraManager::ReadConfig(INIReader* pReader)
{
  m_Camera.MovementSpeed = pReader->GetReal("Camera", "MovementSpeed", 1.0f);
  m_Camera.RotationSpeed = pReader->GetReal("Camera", "RotationSpeed", XM_PI / 4);
  m_Camera.RollSpeed = pReader->GetReal("Camera", "RollSpeed", XM_PI / 8);
  m_Camera.FovSpeed = pReader->GetReal("Camera", "FovSpeed", 5.0f);
}

const std::string CameraManager::GetConfig()
{
  std::string config = "[Camera]\n";
  config += "MovementSpeed = " + std::to_string(m_Camera.MovementSpeed) + "\n";
  config += "RotationSpeed = " + std::to_string(m_Camera.RotationSpeed) + "\n";
  config += "RollSpeed = " + std::to_string(m_Camera.RollSpeed) + "\n";
  config += "FovSpeed = " + std::to_string(m_Camera.FovSpeed) + "\n";

  return config;
}

void CameraManager::UpdateCamera(double dt)
{
  XMVECTOR qPitch = XMQuaternionRotationRollPitchYaw(m_Camera.dPitch * dt * m_Camera.RotationSpeed, 0, 0);
  XMVECTOR qYaw = XMQuaternionRotationRollPitchYaw(0, m_Camera.dYaw* dt * m_Camera.RotationSpeed, 0);
  XMVECTOR qRoll = XMQuaternionRotationRollPitchYaw(0, 0, m_Camera.dRoll* dt * m_Camera.RollSpeed);

  XMVECTOR qRotation = XMLoadFloat4(&m_Camera.Rotation);
  XMVECTOR vPosition = XMLoadFloat3(&m_Camera.Position);

  // Adds delta rotations to camera rotation
  qRotation = XMQuaternionMultiply(qPitch, qRotation);
  qRotation = XMQuaternionMultiply(qRotation, qYaw);
  qRotation = XMQuaternionMultiply(qRoll, qRotation);

  // Make sure it's normalized
  qRotation = XMQuaternionNormalize(qRotation);
  m_Camera.FieldOfView += m_Camera.dFov * dt * m_Camera.FovSpeed;

  // If a camera track is being played, get the current
  // state and overwrite position/rotation/FoV.
  if (m_TrackPlayer.IsPlaying())
  {
    CatmullRomNode resultNode = m_TrackPlayer.PlayForward(dt);

    if (m_TrackPlayer.IsRotationLocked())
      qRotation = XMLoadFloat4(&resultNode.Rotation);

    if (m_TrackPlayer.IsFovLocked())
      m_Camera.FieldOfView = resultNode.FieldOfView;

    vPosition = XMLoadFloat3(&resultNode.Position);
  }
  // Create rotation matrix
  XMMATRIX rotMatrix = XMMatrixRotationQuaternion(qRotation);

  // Add delta positions
  if (!m_TrackPlayer.IsPlaying())
  {
    vPosition += m_Camera.dX * rotMatrix.r[0] * dt * m_Camera.MovementSpeed;
    vPosition += m_Camera.dY * rotMatrix.r[1] * dt * m_Camera.MovementSpeed;
    vPosition += m_Camera.dZ * rotMatrix.r[2] * dt * m_Camera.MovementSpeed;
  }

  // Store results
  rotMatrix.r[3] = vPosition;
  rotMatrix.r[3].m128_f32[3] = 1.0f;

  m_Camera.Matrix = rotMatrix;
  XMStoreFloat3(&m_Camera.Position, vPosition);
  XMStoreFloat4(&m_Camera.Rotation, qRotation);
  XMStoreFloat4x4(&m_Camera.Transform, rotMatrix);

  m_Camera.dX = 0;
  m_Camera.dY = 0;
  m_Camera.dZ = 0;
  m_Camera.dPitch = 0;
  m_Camera.dYaw = 0;
  m_Camera.dRoll = 0;
  m_Camera.dFov = 0;
}

void CameraManager::UpdateInput(double dt)
{
  InputSystem* pInput = g_mainHandle->GetInputSystem();
  if (!g_hasFocus || g_mainHandle->GetUI()->HasKeyboardFocus())
    return;

  m_Camera.dX = pInput->GetActionState(Camera_Right) - pInput->GetActionState(Camera_Left);
  m_Camera.dY = pInput->GetActionState(Camera_Up) - pInput->GetActionState(Camera_Down);
  m_Camera.dZ = pInput->GetActionState(Camera_Forward) - pInput->GetActionState(Camera_Backward);

  m_Camera.dPitch = pInput->GetActionState(Camera_PitchDown) - pInput->GetActionState(Camera_PitchUp);
  m_Camera.dYaw = pInput->GetActionState(Camera_YawRight) - pInput->GetActionState(Camera_YawLeft);
  m_Camera.dRoll = pInput->GetActionState(Camera_RollLeft) - pInput->GetActionState(Camera_RollRight);
  m_Camera.dFov = pInput->GetActionState(Camera_IncFov) - pInput->GetActionState(Camera_DecFov);

  if (m_KbmDisabled && !g_mainHandle->GetUI()->IsEnabled())
  {
    XMFLOAT3 mouseState = pInput->GetMouseState();
    m_Camera.dPitch += mouseState.y * dt;
    m_Camera.dYaw += mouseState.x * dt;
    m_Camera.dFov += mouseState.z * dt;
  }
}

void CameraManager::ToggleCamera()
{
  // If first enable, fetch game camera location
  if (m_FirstEnable || m_AutoReset)
  {
    m_Camera.Rotation = XMFLOAT4(0, 0, 0, 1);
    m_Camera.Position = XMFLOAT3(&m_ResetMatrix.m[3][0]);
    m_Camera.Matrix.r[3] = XMLoadFloat3(&m_Camera.Position);
    m_FirstEnable = false;
  }

  m_CameraEnabled = !m_CameraEnabled;
}

void CameraManager::ResetCamera()
{
  // There's a few ways you can do this
  // Best way might depend on the game

  // In BF tools camera is toggled off, then Sleep() for a few
  // 100 ms, fetch the game camera location again and reset to
  // that and then re-enable camera again.

  if (!m_CameraEnabled) return;
  m_CameraEnabled = false;
  Sleep(100);

  m_Camera.Position = XMFLOAT3(&m_ResetMatrix.m[3][0]);
  m_Camera.Rotation = XMFLOAT4(0, 0, 0, 1);

  m_CameraEnabled = true;
}