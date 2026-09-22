#include "epg_recording_match.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace dispatcharr::recording
{
namespace
{
std::string NormalizeTitle(const std::string& title)
{
  std::string normalized;
  normalized.reserve(title.size());
  bool pendingSpace = false;
  for (const unsigned char character : title)
  {
    if (std::isalnum(character))
    {
      if (pendingSpace && !normalized.empty())
        normalized.push_back(' ');
      normalized.push_back(static_cast<char>(std::tolower(character)));
      pendingSpace = false;
    }
    else
      pendingSpace = !normalized.empty();
  }
  return normalized;
}
}

unsigned int MatchRecordingToEpg(const std::vector<xtream::ChannelEpg>& epgData,
                                 int kodiChannelUid,
                                 time_t recordingStart,
                                 time_t recordingEnd,
                                 const std::string& recordingTitle)
{
  if (kodiChannelUid <= 0 || recordingStart <= 0 || recordingEnd <= recordingStart)
    return 0;

  const auto channelId = std::to_string(kodiChannelUid);
  const std::string normalizedRecordingTitle = NormalizeTitle(recordingTitle);
  unsigned int bestUid = 0;
  int64_t bestScore = std::numeric_limits<int64_t>::min();

  for (const auto& channel : epgData)
  {
    if (channel.id != channelId)
      continue;
    for (const auto& item : channel.entries)
    {
      const auto& entry = item.second;
      const time_t overlapStart = std::max(recordingStart, entry.startTime);
      const time_t overlapEnd = std::min(recordingEnd, entry.endTime);
      const int64_t overlap = static_cast<int64_t>(overlapEnd - overlapStart);
      if (overlap <= 0)
        continue;

      const int64_t recordingDuration = recordingEnd - recordingStart;
      const int64_t entryDuration = entry.endTime - entry.startTime;
      const int64_t shorterDuration = std::min(recordingDuration, entryDuration);
      const bool titleMatches = !normalizedRecordingTitle.empty() &&
                                normalizedRecordingTitle == NormalizeTitle(entry.title);
      const bool substantialOverlap = overlap * 2 >= shorterDuration;
      const bool closeBoundaries = std::llabs(recordingStart - entry.startTime) <= 120 &&
                                   std::llabs(recordingEnd - entry.endTime) <= 120;
      const bool acceptable = normalizedRecordingTitle.empty()
                                  ? closeBoundaries
                                  : titleMatches && substantialOverlap;
      if (!acceptable)
        continue;

      // Exact title matches take precedence; overlap chooses between repeated
      // adjacent programmes with the same title.
      const int64_t score = overlap + (titleMatches ? 1000000000LL : 0LL);
      if (score > bestScore)
      {
        bestScore = score;
        bestUid = static_cast<unsigned int>(entry.startTime);
      }
    }
    break;
  }
  return bestUid;
}

} // namespace dispatcharr::recording
