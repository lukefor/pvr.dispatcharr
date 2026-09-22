#include "hls_playlist.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace dispatcharr::recording
{
namespace
{
std::string Trim(std::string value)
{
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
    value.pop_back();
  size_t first = 0;
  while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first])))
    ++first;
  return value.substr(first);
}
}

bool ParseHlsPlaylist(const std::string& text, HlsPlaylist& playlist, std::string& error)
{
  playlist = {};
  error.clear();
  std::istringstream input(text);
  std::string line;
  int64_t sequence = 0;
  double pendingDuration = -1.0;
  bool header = false;

  while (std::getline(input, line))
  {
    line = Trim(std::move(line));
    if (line.empty())
      continue;
    if (line == "#EXTM3U")
    {
      header = true;
      continue;
    }
    if (line.rfind("#EXT-X-MEDIA-SEQUENCE:", 0) == 0)
    {
      try { sequence = std::stoll(line.substr(22)); }
      catch (...) { error = "invalid media sequence"; return false; }
      continue;
    }
    if (line.rfind("#EXT-X-TARGETDURATION:", 0) == 0)
    {
      try { playlist.targetDurationSeconds = std::max(1, std::stoi(line.substr(22))); }
      catch (...) { error = "invalid target duration"; return false; }
      continue;
    }
    if (line.rfind("#EXTINF:", 0) == 0)
    {
      const size_t comma = line.find(',', 8);
      try { pendingDuration = std::stod(line.substr(8, comma == std::string::npos ? comma : comma - 8)); }
      catch (...) { error = "invalid segment duration"; return false; }
      continue;
    }
    if (line == "#EXT-X-ENDLIST")
    {
      playlist.complete = true;
      continue;
    }
    if (line.front() == '#')
      continue;
    if (pendingDuration < 0.0)
    {
      error = "segment URI without EXTINF";
      return false;
    }
    playlist.segments.push_back({sequence++, pendingDuration, std::move(line)});
    pendingDuration = -1.0;
  }

  if (!header)
    error = "missing EXTM3U header";
  else if (playlist.segments.empty())
    error = "playlist contains no complete segments";
  return error.empty();
}

} // namespace dispatcharr::recording
