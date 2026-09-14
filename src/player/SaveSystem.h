#pragma once
#include <DirectXMath.h>
#include <array>
#include <filesystem>
#include <string>
namespace player {
struct SaveData {
  int version = 1;
  DirectX::XMFLOAT3 position{};
  float yaw = 0, pitch = 0, playSeconds = 0;
  bool heroineJoined = false, findingEmergency = false, clueCollected = false,
       managementUnlocked = false, managementEntered = false,
       archComplete = false, blackoutStarted = false,
       blackoutWritingSeen = false, blackoutHandsSeen = false,
       blackoutFishSeen = false,
       powerRestored = false, terraceRestCompleted = false,
       manualCollected = false, facilityPasswordCollected = false,
       staffDoorOpen = false, beachDecisionPoint = false;
  int heroTankLightColor = 0;
  std::string location;
};
struct SaveSlotInfo {
  bool occupied = false;
  float playSeconds = 0;
  std::string location, timestamp;
};
class SaveSystem {
public:
  static constexpr int kSlotCount = 6;
  SaveSystem();
  bool Save(int slot, const SaveData &data);
  bool Load(int slot, SaveData &data) const;
  std::array<SaveSlotInfo, kSlotCount> Inspect() const;

private:
  std::filesystem::path SlotPath(int slot) const;
  std::filesystem::path directory_;
};
} // namespace player
