#include "recording/epg_recording_match.h"

#include <cassert>

int main()
{
  xtream::ChannelEpg channel;
  channel.id = "42";
  channel.entries.emplace(1000, xtream::EpgEntry{"", 1000, 1600, "Morning News"});
  channel.entries.emplace(1600, xtream::EpgEntry{"", 1600, 2200, "Morning News"});
  const std::vector<xtream::ChannelEpg> epg{channel};

  assert(dispatcharr::recording::MatchRecordingToEpg(
             epg, 42, 990, 1610, "Morning News!") == 1000);
  assert(dispatcharr::recording::MatchRecordingToEpg(
             epg, 42, 1600, 2200, "Morning News") == 1600);
  assert(dispatcharr::recording::MatchRecordingToEpg(
             epg, 43, 1000, 1600, "Morning News") == 0);
  assert(dispatcharr::recording::MatchRecordingToEpg(
             epg, 42, 1100, 1500, "Different Programme") == 0);
  return 0;
}
