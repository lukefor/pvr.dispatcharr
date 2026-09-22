#pragma once

#include "recorded_stream.h"

#include <cstdint>
#include <mutex>

namespace dispatcharr { class Client; }

namespace dispatcharr::recording
{

// Serves a completed recording's file straight from Dispatcharr via
// authenticated HTTP Range requests. Every read goes through Client::Request(),
// so an expired JWT is transparently refreshed and retried instead of failing
// the whole playback the way a single static "?token=" URL handed to Kodi's
// player would.
class RemoteFileRecordedStream : public IRecordedStream
{
public:
  explicit RemoteFileRecordedStream(Client& client);

  bool Open(int recordingId);
  void Close() override;
  int Read(unsigned char* buffer, unsigned int size) override;
  int64_t Seek(int64_t offset, int whence) override;
  int64_t Length() const override;
  bool IsOpen() const override;

private:
  Client& m_client;
  mutable std::mutex m_mutex;
  int m_recordingId = 0;
  int64_t m_position = 0;
  int64_t m_length = -1;
  bool m_open = false;
};

} // namespace dispatcharr::recording
