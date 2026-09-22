#include "recording/hls_playlist.h"

#include <cassert>
#include <cmath>
#include <string>

int main()
{
  const std::string input =
      "#EXTM3U\r\n"
      "#EXT-X-VERSION:6\r\n"
      "#EXT-X-TARGETDURATION:5\r\n"
      "#EXT-X-MEDIA-SEQUENCE:17\r\n"
      "#EXT-X-DISCONTINUITY\r\n"
      "#EXTINF:5.406,\r\n"
      "seg_00017.ts\r\n"
      "#EXT-X-PROGRAM-DATE-TIME:2026-09-12T00:00:00Z\r\n"
      "#EXTINF:4.071,\r\n"
      "https://example.invalid/hls/seg_00018.ts?token=secret\r\n"
      "#EXT-X-ENDLIST\r\n";
  dispatcharr::recording::HlsPlaylist playlist;
  std::string error;
  assert(dispatcharr::recording::ParseHlsPlaylist(input, playlist, error));
  assert(error.empty());
  assert(playlist.targetDurationSeconds == 5);
  assert(playlist.complete);
  assert(playlist.segments.size() == 2);
  assert(playlist.segments[0].sequence == 17);
  assert(std::abs(playlist.segments[0].durationSeconds - 5.406) < 0.0001);
  assert(playlist.segments[1].sequence == 18);

  dispatcharr::recording::HlsPlaylist invalid;
  assert(!dispatcharr::recording::ParseHlsPlaylist("#EXTINF:5,\nseg.ts\n", invalid, error));
  return 0;
}
