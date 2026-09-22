#pragma once

#include <cstdint>

namespace dispatcharr::recording
{

// Common interface Kodi's OpenRecordedStream/Read/Seek/Length callbacks are
// routed through, regardless of whether the recording is still being written
// (GrowingRecordedStream) or already finished (RemoteFileRecordedStream).
class IRecordedStream
{
public:
  virtual ~IRecordedStream() = default;
  virtual void Close() = 0;
  virtual int Read(unsigned char* buffer, unsigned int size) = 0;
  virtual int64_t Seek(int64_t offset, int whence) = 0;
  virtual int64_t Length() const = 0;
  // Only meaningful for streams whose duration isn't implied by Length()
  // (e.g. a growing recording, where total bytes keeps changing). Returns 0
  // when the caller should fall back to demuxer-derived timing instead.
  virtual int64_t DurationMicroseconds() const { return 0; }
  virtual bool IsOpen() const = 0;
};

} // namespace dispatcharr::recording
