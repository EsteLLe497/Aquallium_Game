#include "SaveSystem.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace player {
namespace {
std::string Now() {
  const auto now =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm tm{};
  localtime_s(&tm, &now);
  std::ostringstream out;
  out << std::put_time(&tm, "%Y/%m/%d %H:%M");
  return out.str();
}
bool Bool(const std::string &v) { return v == "1" || v == "true"; }
} // namespace
SaveSystem::SaveSystem() {
  wchar_t *local = nullptr;
  size_t count = 0;
  _wdupenv_s(&local, &count, L"LOCALAPPDATA");
  directory_ = local ? std::filesystem::path(local)
                     : std::filesystem::current_path() / L"save";
  if (local)
    free(local);
  directory_ /= L"Aquallium_Game";
#if defined(_DEBUG)
  directory_ /= L"Debug";
#else
  directory_ /= L"Release";
#endif
  std::error_code ec;
  std::filesystem::create_directories(directory_, ec);
}
std::filesystem::path SaveSystem::SlotPath(int slot) const {
  return directory_ / (L"slot" + std::to_wstring(slot + 1) + L".sav");
}
bool SaveSystem::Save(int slot, const SaveData &d) {
  if (slot < 0 || slot >= kSlotCount)
    return false;
  std::ofstream out(SlotPath(slot), std::ios::trunc);
  if (!out)
    return false;
  out << "version=" << d.version << '\n'
      << "timestamp=" << Now() << '\n'
      << "location=" << d.location << '\n'
      << "play=" << d.playSeconds << '\n'
      << "x=" << d.position.x << '\n'
      << "y=" << d.position.y << '\n'
      << "z=" << d.position.z << '\n'
      << "yaw=" << d.yaw << '\n'
      << "pitch=" << d.pitch << '\n'
      << "heroine=" << d.heroineJoined << '\n'
      << "emergency=" << d.findingEmergency << '\n'
      << "clue=" << d.clueCollected << '\n'
      << "management=" << d.managementUnlocked << '\n'
      << "managementEntered=" << d.managementEntered << '\n'
      << "arch=" << d.archComplete << '\n'
      << "blackout=" << d.blackoutStarted << '\n'
      << "blackoutWriting=" << d.blackoutWritingSeen << '\n'
      << "blackoutFish=" << d.blackoutFishSeen << '\n'
      << "powerRestored=" << d.powerRestored << '\n'
      << "terraceRest=" << d.terraceRestCompleted << '\n'
      << "manual=" << d.manualCollected << '\n'
      << "facilityPassword=" << d.facilityPasswordCollected << '\n'
      << "staffDoor=" << d.staffDoorOpen << '\n'
      << "beachDecision=" << d.beachDecisionPoint << '\n'
      << "heroTankLight=" << d.heroTankLightColor << '\n';
  return bool(out);
}
bool SaveSystem::Load(int slot, SaveData &d) const {
  if (slot < 0 || slot >= kSlotCount)
    return false;
  std::ifstream in(SlotPath(slot));
  if (!in)
    return false;
  std::string line;
  while (std::getline(in, line)) {
    const auto at = line.find('=');
    if (at == std::string::npos)
      continue;
    const auto k = line.substr(0, at), v = line.substr(at + 1);
    try {
      if (k == "version")
        d.version = std::stoi(v);
      else if (k == "location")
        d.location = v;
      else if (k == "play")
        d.playSeconds = std::stof(v);
      else if (k == "x")
        d.position.x = std::stof(v);
      else if (k == "y")
        d.position.y = std::stof(v);
      else if (k == "z")
        d.position.z = std::stof(v);
      else if (k == "yaw")
        d.yaw = std::stof(v);
      else if (k == "pitch")
        d.pitch = std::stof(v);
      else if (k == "heroine")
        d.heroineJoined = Bool(v);
      else if (k == "emergency")
        d.findingEmergency = Bool(v);
      else if (k == "clue")
        d.clueCollected = Bool(v);
      else if (k == "management")
        d.managementUnlocked = Bool(v);
      else if (k == "managementEntered")
        d.managementEntered = Bool(v);
      else if (k == "arch")
        d.archComplete = Bool(v);
      else if (k == "blackout")
        d.blackoutStarted = Bool(v);
      else if (k == "blackoutWriting")
        d.blackoutWritingSeen = Bool(v);
      else if (k == "blackoutFish")
        d.blackoutFishSeen = Bool(v);
      else if (k == "powerRestored")
        d.powerRestored = Bool(v);
      else if (k == "terraceRest")
        d.terraceRestCompleted = Bool(v);
      else if (k == "manual")
        d.manualCollected = Bool(v);
      else if (k == "facilityPassword")
        d.facilityPasswordCollected = Bool(v);
      else if (k == "staffDoor")
        d.staffDoorOpen = Bool(v);
      else if (k == "beachDecision")
        d.beachDecisionPoint = Bool(v);
      else if (k == "heroTankLight")
        d.heroTankLightColor = std::clamp(std::stoi(v), 0, 2);
    } catch (...) {
      return false;
    }
  }
  return d.version == 1;
}
std::array<SaveSlotInfo, SaveSystem::kSlotCount> SaveSystem::Inspect() const {
  std::array<SaveSlotInfo, kSlotCount> result{};
  for (int i = 0; i < kSlotCount; ++i) {
    std::ifstream in(SlotPath(i));
    if (!in)
      continue;
    result[i].occupied = true;
    std::string line;
    while (std::getline(in, line)) {
      const auto at = line.find('=');
      if (at == std::string::npos)
        continue;
      const auto k = line.substr(0, at), v = line.substr(at + 1);
      if (k == "timestamp")
        result[i].timestamp = v;
      else if (k == "location")
        result[i].location = v;
      else if (k == "play")
        try {
          result[i].playSeconds = std::stof(v);
        } catch (...) {
          result[i].occupied = false;
        }
    }
  }
  return result;
}
} // namespace player
