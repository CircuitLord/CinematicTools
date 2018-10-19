#include "TrackPlayer.h"
#include "../Globals.h"
#include "../Util/Util.h"
#include "../Util/ImGuiEXT.h"
#include "../resource.h"

using namespace DirectX;

TrackPlayer::TrackPlayer() :
  m_IsPlaying(false),
  m_LockRotation(true),
  m_LockFieldOfView(false),
  m_ManualPlay(false),
  m_NodeTimeSpan(3.0f),
  m_CurrentNode(0),
  m_CurrentTime(0),
  m_RunningId(2)
{
  m_Tracks.emplace_back("Track #1");
  m_TrackNames.push_back(m_Tracks[0].Name.c_str());

  m_pCameraModel = g_mainHandle->GetRenderer()->CreateModelFromResource(IDR_OBJ_CAMERA);
}

TrackPlayer::~TrackPlayer()
{

}

void TrackPlayer::CreateNode(Camera const& camera)
{
  if (m_IsPlaying) return;

  CatmullRomNode newNode;

  newNode.FieldOfView = camera.FieldOfView;
  newNode.Rotation = camera.Rotation;
  newNode.Position = camera.Position;

  newNode.Transform = XMMatrixRotationQuaternion(XMLoadFloat4(&camera.Rotation));
  newNode.Transform.r[3] = XMLoadFloat3(&camera.Position);
  newNode.Transform.r[3].m128_f32[3] = 1.0f;

  newNode.TimeStamp = 0;

  size_t nodes = m_Tracks[m_SelectedTrack].Nodes.size();
  if (nodes > 0)
    newNode.TimeStamp = m_Tracks[m_SelectedTrack].Nodes[nodes - 1].TimeStamp + 1.0f;

  m_Tracks[m_SelectedTrack].Nodes.push_back(newNode);
  UpdateNodeBuffers();
  util::log::Write("Node created, total nodes: %d", m_Tracks[m_SelectedTrack].Nodes.size());
}

void TrackPlayer::DeleteNode()
{
  if (m_IsPlaying) return;

  std::vector<CatmullRomNode>& nodes = m_Tracks[m_SelectedTrack].Nodes;
  if (nodes.size() == 0) return;

  nodes.erase(nodes.begin() + (nodes.size() - 1));
  UpdateNodeBuffers();
  util::log::Write("Deleted node, remaining nodes %d", nodes.size());
}

void TrackPlayer::Toggle()
{
  if (m_Tracks[m_SelectedTrack].Nodes.size() <= 1)
  {
    util::log::Warning("Can't play a camera track with less than 2 nodes");
    return;
  }

  m_IsPlaying = !m_IsPlaying;
  if (!m_IsPlaying) return;

  m_CurrentTime = 0;
  m_CurrentNode = 0;
}

CatmullRomNode TrackPlayer::PlayForward(double dt, bool ignoreManual /* = false */)
{
  std::vector<CatmullRomNode>& nodes = m_Tracks[m_SelectedTrack].Nodes;

  // Default time from node to node is 1 second.
  // NodeTimeSpan modifies this. 
  float timeMultiplier = (1.f / m_NodeTimeSpan);

  // If manual play is enabled, time is multiplied
  // by input. IgnoreManual is for generating display buffers.
  if (!m_ManualPlay || ignoreManual)
    m_CurrentTime += dt * timeMultiplier;
  else
  {
    float controlMultiplier = g_mainHandle->GetInputSystem()->GetActionState(Action::Camera_Up);
    controlMultiplier -= g_mainHandle->GetInputSystem()->GetActionState(Action::Camera_Down);
    m_CurrentTime += dt * timeMultiplier * controlMultiplier;
  }

  std::vector<CatmullRomNode>::iterator n0, n1, n2, n3;

  bool goForward = false; // Time is increasing, should we move on to the next nodes?
  bool goBackward = false; // Time is decreasing, should we move on to the previous nodes?

  // Detect if we should move onto the next/previous node. If we're at the
  // start or the end, return those nodes.
  while ((goForward = m_CurrentTime >= nodes[m_CurrentNode + 1].TimeStamp) ||
    (goBackward = m_CurrentTime < nodes[m_CurrentNode].TimeStamp))
  {
    if (goForward)
    {
      if (m_CurrentNode + 1 < nodes.size() - 1)
      {
        m_CurrentNode++;
      }
      else
      {
        return nodes[m_CurrentNode + 1];
      }
    }
    else if (goBackward)
    {
      if (m_CurrentNode - 1 >= 0)
      {
        m_CurrentNode -= 1;
      }
      else
      {
        m_CurrentTime = 0;
        return nodes[0];
      }
    }
  }

  n1 = nodes.begin() + m_CurrentNode;
  n2 = n1 + 1;
  n0 = m_CurrentNode > 0 ? n1 - 1 : n1;
  n3 = m_CurrentNode + 1 < nodes.size() - 1 ? n2 + 1 : n2;

  double mu = (m_CurrentTime - n1->TimeStamp) / (n2->TimeStamp - n1->TimeStamp);

  XMVECTOR qRot0 = XMLoadFloat4(&n0->Rotation);
  XMVECTOR qRot1 = XMLoadFloat4(&n1->Rotation);
  XMVECTOR qRot2 = XMLoadFloat4(&n2->Rotation);
  XMVECTOR qRot3 = XMLoadFloat4(&n3->Rotation);

  XMVECTOR vPos0 = XMLoadFloat3(&n0->Position);
  XMVECTOR vPos1 = XMLoadFloat3(&n1->Position);
  XMVECTOR vPos2 = XMLoadFloat3(&n2->Position);
  XMVECTOR vPos3 = XMLoadFloat3(&n3->Position);

  CatmullRomNode resultNode;

  resultNode.FieldOfView = util::math::CatmullRomInterpolate(n0->FieldOfView,
    n1->FieldOfView,
    n2->FieldOfView,
    n3->FieldOfView, mu);

  XMVECTOR resultRot = XMVectorCatmullRom(qRot0, qRot1, qRot2, qRot3, mu);
  XMVECTOR resultPos = XMVectorCatmullRom(vPos0, vPos1, vPos2, vPos3, mu);

  XMStoreFloat4(&resultNode.Rotation, XMQuaternionNormalize(resultRot));
  XMStoreFloat3(&resultNode.Position, resultPos);

  return resultNode;
}

void TrackPlayer::DrawUI()
{
  ImGui::Text("Camera tracks");
  ImGui::Combo("##CameraTrackList", (int*)&m_SelectedTrack, &m_TrackNames[0], m_TrackNames.size());
  if (ImGui::Button("Create", ImVec2(95, 25)))
    CreateTrack();
  ImGui::SameLine(0, 10);
  if (ImGui::Button("Delete", ImVec2(95, 25)))
    DeleteTrack();
  ImGui::Dummy(ImVec2(0, 5));
  ImGui::Dummy(ImVec2(0, 5));
  ImGui::Text("Time multiplier");
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 10));
  ImGui::InputFloat("##CameraTrackSpeedMultiplier", &m_NodeTimeSpan, 0.1f, 0, 2);

  ImGui::Checkbox("Lock focal length", &m_LockFieldOfView);
  ImGui::Checkbox("Lock rotation", &m_LockRotation);
  ImGui::Checkbox("Play manually", &m_ManualPlay);
  ImGui::PopStyleVar();
}

void TrackPlayer::DrawNodes()
{
  if (m_IsPlaying) return;

  CameraTrack& track = m_Tracks[m_SelectedTrack];
  for (auto& node : track.Nodes)
    g_mainHandle->GetRenderer()->DrawModel(m_pCameraModel.get(), node.Transform, { 1,0,0 });

  if (track.IndexCount > 0)
    g_mainHandle->GetRenderer()->DrawLines(track.Indices.Get(), track.Vertices.Get(), track.IndexCount);
}

void TrackPlayer::CreateTrack()
{
  if (m_IsPlaying) return;

  m_Tracks.emplace_back("Track #" + std::to_string(m_RunningId++));
  m_SelectedTrack = m_Tracks.size() - 1;

  UpdateNameList();
}

void TrackPlayer::DeleteTrack()
{
  if (m_IsPlaying || m_Tracks.size() <= 1) return;

  m_Tracks.erase(m_Tracks.begin() + m_SelectedTrack);
  if (m_SelectedTrack >= m_Tracks.size())
    m_SelectedTrack -= 1;

  UpdateNameList();
}

void TrackPlayer::UpdateNodeBuffers()
{
  CameraTrack& track = m_Tracks[m_SelectedTrack];
  const std::vector<CatmullRomNode>& nodes = track.Nodes;

  track.Vertices.Reset();
  track.Indices.Reset();

  if (nodes.size() < 2)
    return;

  std::vector<VertexPositionColor> verticesVector;
  std::vector<unsigned int> indicesVector;

  VertexPositionColor v1, v2;
  v1.position = nodes[0].Position;
  v1.color = v2.color = XMFLOAT4(1, 0, 0, 1);

  XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), XMLoadFloat4(&nodes[0].Rotation));
  XMStoreFloat3(&v2.position, XMLoadFloat3(&nodes[0].Position) - 0.5f*forward);

  verticesVector.push_back(v2);
  verticesVector.push_back(v1);
  indicesVector.push_back(0);
  indicesVector.push_back(1);

  m_CurrentTime = 0;
  m_CurrentNode = 0;
  int lastNodeVertexIndex = 0;
  int stepCount = 0;

  // Play through the track, save nodes into vertices
  while (m_CurrentTime < nodes[nodes.size() - 1].TimeStamp)
  {
    stepCount++;

    VertexPositionColor nodeVertex, forwardVertex;
    nodeVertex.color = forwardVertex.color = XMFLOAT4(1, 0, 0, 1);

    CatmullRomNode node = PlayForward(0.1f, true);
    nodeVertex.position = node.Position;

    verticesVector.push_back(nodeVertex);
    indicesVector.push_back(verticesVector.size() - 1); // Current node vertex

    lastNodeVertexIndex = verticesVector.size() - 1;

    // Create forward line to indicate 0.5 second of track time
    if (stepCount % 5 == 0)
    {
      XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), XMLoadFloat4(&node.Rotation));
      XMStoreFloat3(&forwardVertex.position, XMLoadFloat3(&node.Position) - 0.5*forward);

      verticesVector.push_back(forwardVertex);
      indicesVector.push_back(verticesVector.size() - 1);
      indicesVector.push_back(verticesVector.size() - 2);
    }
  }

  track.IndexCount = indicesVector.size();
  g_mainHandle->GetRenderer()->CreateVertexIndexBuffers(indicesVector, verticesVector, track.Vertices.GetAddressOf(), track.Indices.GetAddressOf());

  m_CurrentTime = 0;
  m_CurrentNode = 0;
}

void TrackPlayer::UpdateNameList()
{
  m_TrackNames.clear();
  for (auto& track : m_Tracks)
    m_TrackNames.push_back(track.Name.c_str());
}