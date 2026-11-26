#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace bomber {
// [01] Small local ranking model, shared by UI and persistence tests.
struct RankEntry {
    int score = 0, stage = 3, players = 1;
    std::string clearedAt;
    bool legacyConfirmed = false;
};
std::vector<RankEntry> readScores(const std::filesystem::path& path);
bool saveScore(const std::filesystem::path& path, RankEntry entry);
std::string localTimestamp();
std::filesystem::path scoreFileForExecutable(const std::filesystem::path& directory);
} // namespace bomber
