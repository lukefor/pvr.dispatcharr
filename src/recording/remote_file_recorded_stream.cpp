#include "remote_file_recorded_stream.h"

#include "../dispatcharr_client.h"

#include <algorithm>
#include <cstring>
#include <cstdio>

namespace dispatcharr::recording
{

RemoteFileRecordedStream::RemoteFileRecordedStream(Client& client) : m_client(client) {}

bool RemoteFileRecordedStream::Open(int recordingId)
{
  Close();

  // A minimal ranged read both confirms the file is reachable and yields the
  // total size via Content-Range, without downloading the whole recording.
  std::string probe;
  int64_t totalLength = 0;
  if (!m_client.FetchRecordingFileRange(recordingId, 0, 1, probe, totalLength) ||
      totalLength <= 0)
    return false;

  std::lock_guard<std::mutex> lock(m_mutex);
  m_recordingId = recordingId;
  m_position = 0;
  m_length = totalLength;
  m_open = true;
  return true;
}

void RemoteFileRecordedStream::Close()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_open = false;
  m_position = 0;
  m_length = -1;
}

int RemoteFileRecordedStream::Read(unsigned char* buffer, unsigned int size)
{
  if (!buffer || size == 0)
    return 0;

  int recordingId = 0;
  int64_t position = 0;
  int64_t length = 0;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_open)
      return -1;
    if (m_position >= m_length)
      return 0;
    recordingId = m_recordingId;
    position = m_position;
    length = m_length;
  }

  const int64_t remaining = length - position;
  const int64_t wanted = std::min<int64_t>(static_cast<int64_t>(size), remaining);

  std::string data;
  int64_t reportedLength = 0;
  if (!m_client.FetchRecordingFileRange(recordingId, position, wanted, data, reportedLength))
    return -1;
  if (data.empty())
    return 0;

  std::memcpy(buffer, data.data(), data.size());

  std::lock_guard<std::mutex> lock(m_mutex);
  m_position += static_cast<int64_t>(data.size());
  return static_cast<int>(data.size());
}

int64_t RemoteFileRecordedStream::Seek(int64_t offset, int whence)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  int64_t base = 0;
  if (whence == SEEK_CUR)
    base = m_position;
  else if (whence == SEEK_END)
    base = m_length;
  else if (whence != SEEK_SET)
    return -1;

  const int64_t result = base + offset;
  if (result < 0 || result > m_length)
    return -1;
  m_position = result;
  return result;
}

int64_t RemoteFileRecordedStream::Length() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_length;
}

bool RemoteFileRecordedStream::IsOpen() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_open;
}

} // namespace dispatcharr::recording
