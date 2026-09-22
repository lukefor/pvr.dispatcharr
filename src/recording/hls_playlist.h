#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dispatcharr::recording
{

struct HlsSegment
{
  int64_t sequence = 0;
  double durationSeconds = 0.0;
  std::string uri;
};

struct HlsPlaylist
{
  int targetDurationSeconds = 6;
  bool complete = false;
  std::vector<HlsSegment> segments;
};

bool ParseHlsPlaylist(const std::string& text, HlsPlaylist& playlist, std::string& error);

} // namespace dispatcharr::recording
