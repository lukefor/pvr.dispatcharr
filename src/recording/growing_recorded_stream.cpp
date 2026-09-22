#include "growing_recorded_stream.h"

#include "../dispatcharr_client.h"

#include <kodi/General.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>

namespace dispatcharr::recording
{

GrowingRecordedStream::GrowingRecordedStream(Client& client) : m_client(client) {}
GrowingRecordedStream::~GrowingRecordedStream() { Close(); }

void GrowingRecordedStream::CleanupStaleFiles(const std::string& directory)
{
  std::error_code error;
  if (!std::filesystem::exists(directory, error))
    return;
  for (const auto& entry : std::filesystem::directory_iterator(directory, error))
  {
    const std::string name = entry.path().filename().string();
    if (entry.is_regular_file(error) && name.rfind("recording-", 0) == 0 &&
        (entry.path().extension() == ".ts" || entry.path().extension() == ".part"))
      std::filesystem::remove(entry.path(), error);
  }
}

bool GrowingRecordedStream::Open(int recordingId, const std::string& cacheDirectory)
{
  Close();
  std::error_code error;
  std::filesystem::create_directories(cacheDirectory, error);
  if (error)
    return false;

  m_cachePath = (std::filesystem::path(cacheDirectory) /
                 ("recording-" + std::to_string(recordingId) + ".ts")).string();
  m_producer.open(m_cachePath, std::ios::binary | std::ios::trunc);
  if (!m_producer)
    return false;
  m_consumer.open(m_cachePath, std::ios::binary);
  if (!m_consumer)
  {
    m_producer.close();
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_recordingId = recordingId;
    m_position = 0;
    m_cachedBytes = 0;
    m_cachedDuration = 0.0;
    m_targetDuration = 6;
    m_open = true;
    m_closing = false;
    m_complete = false;
    m_failed = false;
    m_downloadedSequences.clear();
  }

  // Give FFmpeg real data to probe before returning. The worker downloads the
  // remainder of the historical recording without pacing it.
  if (!RefreshAndDownload(2) || Length() == 0)
  {
    kodi::Log(ADDON_LOG_ERROR,
              "pvr.dispatcharr: Could not cache initial media for recording %d",
              recordingId);
    Close();
    return false;
  }
  m_worker = std::thread(&GrowingRecordedStream::Worker, this);
  kodi::Log(ADDON_LOG_INFO,
            "pvr.dispatcharr: Opened growing recording %d with %lld cached bytes",
            recordingId, static_cast<long long>(Length()));
  return true;
}

void GrowingRecordedStream::Close()
{
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_open && !m_worker.joinable())
      return;
    m_closing = true;
    m_changed.notify_all();
  }
  if (m_worker.joinable())
    m_worker.join();
  m_consumer.close();
  m_producer.close();
  if (!m_cachePath.empty())
  {
    std::error_code error;
    std::filesystem::remove(m_cachePath, error);
  }
  std::lock_guard<std::mutex> lock(m_mutex);
  m_open = false;
  m_closing = false;
  m_cachePath.clear();
}

bool GrowingRecordedStream::AppendSegment(const HlsSegment& segment, const std::string& data)
{
  m_producer.write(data.data(), static_cast<std::streamsize>(data.size()));
  m_producer.flush();
  if (!m_producer)
    return false;

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_closing)
      return false;
    m_cachedBytes += static_cast<int64_t>(data.size());
    m_cachedDuration += segment.durationSeconds;
    m_downloadedSequences.insert(segment.sequence);
    kodi::Log(ADDON_LOG_DEBUG,
              "pvr.dispatcharr: Cached recording %d segment %lld; bytes=%lld duration=%.3f",
              m_recordingId, static_cast<long long>(segment.sequence),
              static_cast<long long>(m_cachedBytes), m_cachedDuration);
  }
  m_changed.notify_all();
  return true;
}

bool GrowingRecordedStream::RefreshAndDownload(size_t maximumNewSegments)
{
  std::string manifest;
  if (!m_client.FetchActiveRecordingManifest(m_recordingId, manifest))
    return false;
  HlsPlaylist playlist;
  std::string parseError;
  if (!ParseHlsPlaylist(manifest, playlist, parseError))
  {
    kodi::Log(ADDON_LOG_ERROR, "pvr.dispatcharr: Invalid recording HLS playlist: %s",
              parseError.c_str());
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_targetDuration = playlist.targetDurationSeconds;
  }

  size_t appended = 0;
  for (const auto& segment : playlist.segments)
  {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (m_closing)
        return false;
      if (m_downloadedSequences.count(segment.sequence) != 0)
        continue;
    }
    std::string data;
    if (!m_client.DownloadRecordingSegment(m_recordingId, segment.uri, data))
      return false;
    if (!AppendSegment(segment, data))
      return false;
    if (maximumNewSegments != 0 && ++appended >= maximumNewSegments)
      break;
  }

  if (playlist.complete)
  {
    bool allDownloaded = true;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      for (const auto& segment : playlist.segments)
        allDownloaded = allDownloaded && m_downloadedSequences.count(segment.sequence) != 0;
      if (allDownloaded)
        m_complete = true;
    }
    if (allDownloaded)
      m_changed.notify_all();
  }
  return true;
}

void GrowingRecordedStream::Worker()
{
  int consecutiveFailures = 0;
  while (true)
  {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (m_closing || m_complete)
        break;
    }
    if (RefreshAndDownload())
      consecutiveFailures = 0;
    else
    {
      ++consecutiveFailures;
      // Dispatcharr can replace the HLS directory with the final MKV shortly
      // after recording finishes. Do not leave a reader waiting forever if
      // the last manifest is no longer available.
      Recording recording;
      if (m_client.GetRecording(m_recordingId, recording) &&
          (recording.status == "completed" || recording.status == "interrupted"))
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_complete = true;
        m_changed.notify_all();
        break;
      }
      if (consecutiveFailures >= 30)
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_failed = true;
        m_changed.notify_all();
        break;
      }
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    const auto delay = std::chrono::seconds(std::max(1, m_targetDuration / 2));
    m_changed.wait_for(lock, delay, [this]() { return m_closing; });
  }
}

int GrowingRecordedStream::Read(unsigned char* buffer, unsigned int size)
{
  if (!buffer || size == 0)
    return 0;
  std::unique_lock<std::mutex> lock(m_mutex);
  m_changed.wait(lock, [this]() {
    return m_position < m_cachedBytes || m_complete || m_failed || m_closing;
  });
  if (m_position >= m_cachedBytes)
    return m_complete ? 0 : -1;

  const int64_t position = m_position;
  const auto available = static_cast<uint64_t>(m_cachedBytes - position);
  const unsigned int requested = static_cast<unsigned int>(
      std::min<uint64_t>(size, available));
  lock.unlock();

  m_consumer.clear();
  m_consumer.seekg(position, std::ios::beg);
  m_consumer.read(reinterpret_cast<char*>(buffer), requested);
  const int count = static_cast<int>(m_consumer.gcount());
  if (count <= 0)
    return -1;
  lock.lock();
  m_position += count;
  return count;
}

int64_t GrowingRecordedStream::Seek(int64_t offset, int whence)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  const int64_t oldPosition = m_position;
  int64_t base = 0;
  if (whence == SEEK_CUR)
    base = m_position;
  else if (whence == SEEK_END)
    base = m_cachedBytes;
  else if (whence != SEEK_SET)
    return -1;
  if ((offset < 0 && base < -offset) || (offset > 0 && base > m_cachedBytes - offset))
    return -1;
  const int64_t result = base + offset;
  if (result < 0 || result > m_cachedBytes)
    return -1;
  m_position = result;
  kodi::Log(ADDON_LOG_DEBUG,
            "pvr.dispatcharr: Seek recording %d from %lld to %lld (whence=%d, offset=%lld)",
            m_recordingId, static_cast<long long>(oldPosition),
            static_cast<long long>(result), whence, static_cast<long long>(offset));
  return result;
}

int64_t GrowingRecordedStream::Length() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_cachedBytes;
}

int64_t GrowingRecordedStream::DurationMicroseconds() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return static_cast<int64_t>(m_cachedDuration * 1000000.0);
}

bool GrowingRecordedStream::IsOpen() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_open && !m_closing;
}

} // namespace dispatcharr::recording
