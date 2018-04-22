#pragma once
#include "CameraStructs.h"
#include <vector>

class TrackPlayer
{
public:
  TrackPlayer();
  ~TrackPlayer();

  void DrawUI();
  void DrawNodes();

  void CreateNode(Camera const& camera);
  void DeleteNode();

  void Toggle();
  CatmullRomNode PlayForward(double dt, bool ignoreManual = false);

  bool IsPlaying() { return m_IsPlaying; }
  bool IsRotationLocked() { return m_LockRotation; }
  bool IsFovLocked() { return m_LockFieldOfView; }

private:
  void CreateTrack();
  void DeleteTrack();

private:
  bool m_IsPlaying;

  bool m_LockRotation;
  bool m_LockFieldOfView;
  bool m_ManualPlay;
  float m_SpeedMultiplier;

  unsigned int m_CurrentNode;
  float m_CurrentTime;

  std::vector<int> m_Tracks;
  unsigned int m_SelectedTrack;

  std::vector<const char*> m_TrackNames;
  int m_RunningId;

public:
  TrackPlayer(TrackPlayer const&) = delete;
  void operator=(TrackPlayer const&) = delete;

};