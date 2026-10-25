#include "audio_player.h"

#include <algorithm>

namespace {
// The ceiling is a safety net for a device that stalls, not a working limit:
// with the main loop steering the queue it is never approached.  It is stated
// in milliseconds on purpose.  The old cap counted blocks, which silently
// meant 240 ms at twenty milliseconds a block -- and would have meant a
// thrashing 30 ms had the blocks ever been made small.
constexpr double kMaxQueueMs = 120.0;
// Under this, with sound still coming, the device has run out or is about to:
// the main loop hands over samples every two milliseconds or so, so a queue
// this low means a pass came late enough to leave a gap.
constexpr double kDryMs = 1.0;
}  // namespace

AudioPlayer::~AudioPlayer() {
  Close();
}

bool AudioPlayer::Open(unsigned sampleRate) {
  Close();
  WAVEFORMATEX format{};
  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = 1;
  format.nSamplesPerSec = sampleRate;
  format.wBitsPerSample = 16;
  format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
  format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
  if (waveOutOpen(&device_, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) !=
      MMSYSERR_NOERROR) {
    device_ = nullptr;
    return false;
  }
  sampleRate_ = sampleRate;
  submitted_ = 0;
  queued_ = 0;
  return true;
}

double AudioPlayer::QueuedMs() const {
  if (!device_ || sampleRate_ == 0) return 0.0;
  const double perSample = 1000.0 / static_cast<double>(sampleRate_);
  MMTIME time{};
  time.wType = TIME_SAMPLES;
  // waveOutGetPosition counts what has actually left, so it sees the driver's
  // own buffering as well as ours; the block bookkeeping only sees ours.
  if (waveOutGetPosition(device_, &time, sizeof(time)) == MMSYSERR_NOERROR &&
      time.wType == TIME_SAMPLES) {
    // The device reports 32 bits of sample position; mask our own count the
    // same way so the difference stays right across the wrap at 24 hours.
    const uint32_t handed = static_cast<uint32_t>(submitted_);
    const int32_t outstanding = static_cast<int32_t>(handed - time.u.sample);
    if (outstanding >= 0)
      return (static_cast<double>(outstanding) + staging_.size()) * perSample;
  }
  return static_cast<double>(queued_ + staging_.size()) * perSample;
}

// What the device itself still has to play: QueuedMs without staging_, which
// the device has not got yet.  Counting staging_ hid a starved device behind
// up to a block of samples still being collected.  A position past what was
// handed over is read as empty -- whatever the driver played there, it was
// not ours.  Also returns the raw position, for the gap clock in Submit.
double AudioPlayer::DeviceQueuedMs(uint32_t& position) const {
  MMTIME time{};
  time.wType = TIME_SAMPLES;
  if (waveOutGetPosition(device_, &time, sizeof(time)) != MMSYSERR_NOERROR ||
      time.wType != TIME_SAMPLES) {
    position = 0;
    return static_cast<double>(queued_) * 1000.0 / sampleRate_;
  }
  position = time.u.sample;
  const int32_t outstanding =
      static_cast<int32_t>(static_cast<uint32_t>(submitted_) - time.u.sample);
  return outstanding > 0 ? outstanding * 1000.0 / sampleRate_ : 0.0;
}

void AudioPlayer::Reap() {
  while (!blocks_.empty() && (blocks_.front()->header.dwFlags & WHDR_DONE)) {
    queued_ -= blocks_.front()->samples.size();
    waveOutUnprepareHeader(device_, &blocks_.front()->header, sizeof(WAVEHDR));
    blocks_.pop_front();
  }
}

void AudioPlayer::WriteBlock(std::vector<int16_t> samples) {
  if (!device_ || samples.empty()) return;
  auto block = std::make_unique<Block>();
  block->samples = std::move(samples);
  block->header.lpData = reinterpret_cast<LPSTR>(block->samples.data());
  block->header.dwBufferLength =
      static_cast<DWORD>(block->samples.size() * sizeof(int16_t));
  if (waveOutPrepareHeader(device_, &block->header, sizeof(WAVEHDR)) !=
      MMSYSERR_NOERROR) return;
  const std::size_t count = block->samples.size();
  if (waveOutWrite(device_, &block->header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
    waveOutUnprepareHeader(device_, &block->header, sizeof(WAVEHDR));
    return;
  }
  submitted_ += count;
  queued_ += count;
  blocks_.push_back(std::move(block));
}

void AudioPlayer::Submit(std::vector<int16_t> samples) {
  if (!device_) return;
  Reap();
  // A pass with nothing to hand over is a machine switched off; the queue it
  // lets drain is not a gap, so counting starts again with the next block.
  if (samples.empty()) {
    playing_ = false;
    dryNow_ = false;
    clockRunning_ = false;
  } else if (playing_) {
    uint32_t position = 0;
    const double device = DeviceQueuedMs(position);
    if (statCount_ == 0 || device < statMin_) statMin_ = device;
    if (statCount_ == 0 || device > statMax_) statMax_ = device;
    statSum_ += device;
    ++statCount_;
    // Counted once per gap, not once per pass spent in it.
    const bool dry = device < kDryMs;
    if (dry && !dryNow_) ++statDry_;
    dryNow_ = dry;
    const auto now = std::chrono::steady_clock::now();
    if (!clockRunning_) {
      clockRunning_ = true;
      clockStart_ = now;
      clockStartPosition_ = position;
      lastMinDeficit_ = 0.0;
      haveWindowMin_ = false;
    }
    const double wall =
        std::chrono::duration<double, std::milli>(now - clockStart_).count();
    const double played =
        static_cast<uint32_t>(position - clockStartPosition_) * 1000.0 / sampleRate_;
    const double deficit = wall - played;
    if (!haveWindowMin_ || deficit < windowMinDeficit_) windowMinDeficit_ = deficit;
    haveWindowMin_ = true;
  }
  staging_.insert(staging_.end(), samples.begin(), samples.end());
  const std::size_t block = BlockSamples();
  if (block == 0) return;
  const double perBlock = static_cast<double>(block) * 1000.0 / sampleRate_;
  double queued = QueuedMs();
  while (staging_.size() >= block) {
    std::vector<int16_t> chunk(staging_.begin(),
                               staging_.begin() + static_cast<long>(block));
    staging_.erase(staging_.begin(), staging_.begin() + static_cast<long>(block));
    // Over the ceiling the excess is dropped rather than waited out.  The old
    // code slept here, and that stalled the whole main loop: on this machine
    // sound is the entire user interface, but so is the keyboard, and freezing
    // both to protect a buffer trades a glitch for an unresponsive machine.
    if (queued > kMaxQueueMs) {
      ++statDropped_;
      continue;
    }
    WriteBlock(std::move(chunk));
    playing_ = true;
    queued += perBlock;
  }
}

AudioPlayer::Stats AudioPlayer::TakeStats() {
  Stats stats;
  stats.minQueueMs = statMin_;
  stats.maxQueueMs = statMax_;
  stats.meanQueueMs = statCount_ ? statSum_ / static_cast<double>(statCount_) : 0.0;
  stats.dry = statDry_;
  stats.dropped = statDropped_;
  if (haveWindowMin_) {
    stats.gapMs = std::max(0.0, windowMinDeficit_ - lastMinDeficit_);
    lastMinDeficit_ = windowMinDeficit_;
    haveWindowMin_ = false;
  }
  statMin_ = statMax_ = statSum_ = 0.0;
  statCount_ = statDry_ = statDropped_ = 0;
  return stats;
}

void AudioPlayer::Close() {
  if (!device_) return;
  if (!staging_.empty()) {
    WriteBlock(std::move(staging_));
    staging_.clear();
  }
  waveOutReset(device_);
  for (auto& block : blocks_)
    waveOutUnprepareHeader(device_, &block->header, sizeof(WAVEHDR));
  blocks_.clear();
  waveOutClose(device_);
  device_ = nullptr;
  sampleRate_ = 0;
  submitted_ = 0;
  queued_ = 0;
  playing_ = false;
  dryNow_ = false;
  clockRunning_ = false;
}
