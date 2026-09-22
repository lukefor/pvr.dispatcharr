#pragma once

#include "../xtream_client.h"

#include <ctime>
#include <string>
#include <vector>

namespace dispatcharr::recording
{

unsigned int MatchRecordingToEpg(const std::vector<xtream::ChannelEpg>& epgData,
                                 int kodiChannelUid,
                                 time_t recordingStart,
                                 time_t recordingEnd,
                                 const std::string& recordingTitle);

} // namespace dispatcharr::recording
