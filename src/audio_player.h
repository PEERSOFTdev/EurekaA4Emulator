#ifndef EUREKA_AUDIO_PLAYER_H
#define EUREKA_AUDIO_PLAYER_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>

class AudioPlayer {
 public:
  AudioPlayer() = default;
  ~AudioPlayer();

  bool Open(unsigned sampleRate);
  void Submit(std::vector<int16_t> samples);
  void Close();

  bool Ready() const { return device_ != nullptr; }
  // Milliseconds of sound handed over but not yet played, staging included.
  // This is the latency the listener hears, and it is what the main loop
  // steers the emulated clock by, so it is asked of the device itself rather
  // than inferred from how many blocks happen to be outstanding.
  double QueuedMs() const;

  // How the queue fared since the last call, for the diagnostics dump: an
  // audible gap is a queue that ran dry with sound still coming, and nothing
  // else in the program can tell one from silence the guest chose.
  struct Stats {
    // What the device itself held, without samples still being collected
    // into a block.
    double minQueueMs = 0.0;
    double meanQueueMs = 0.0;
    double maxQueueMs = 0.0;
    // Times the device ran out while there were samples to play.
    uint64_t dry = 0;
    // How much less the device played than the wall clock ran: the length of
    // the gaps, however short each one was.
    double gapMs = 0.0;
    // Blocks thrown away over the ceiling -- a skip in the sound.
    uint64_t dropped = 0;
  };
  Stats TakeStats();

 private:
  struct Block {
    WAVEHDR header{};
    std::vector<int16_t> samples;
  };

  // Five milliseconds.  Twenty was the old figure and it put a whole block of
  // latency into staging_ before anything reached the device at all.
  std::size_t BlockSamples() const { return sampleRate_ / 200; }

  void Reap();
  void WriteBlock(std::vector<int16_t> samples);
  double DeviceQueuedMs(uint32_t& position) const;

  HWAVEOUT device_ = nullptr;
  unsigned sampleRate_ = 0;
  uint64_t submitted_ = 0;
  uint64_t queued_ = 0;
  std::deque<std::unique_ptr<Block>> blocks_;
  std::vector<int16_t> staging_;

  // Accumulated for TakeStats.  Counting starts with the first block: an
  // empty queue before anything was ever written is not a gap.
  bool playing_ = false;
  bool dryNow_ = false;
  double statMin_ = 0.0;
  double statMax_ = 0.0;
  double statSum_ = 0.0;
  uint64_t statCount_ = 0;
  uint64_t statDry_ = 0;
  uint64_t statDropped_ = 0;
  // The gap clock: wall time against device position since playing started.
  // Their difference only grows while the device starves; the position's
  // coarse steps make it jitter, which the minimum over each TakeStats window
  // takes out (it always catches a step just taken).
  bool clockRunning_ = false;
  std::chrono::steady_clock::time_point clockStart_;
  uint32_t clockStartPosition_ = 0;
  double lastMinDeficit_ = 0.0;
  double windowMinDeficit_ = 0.0;
  bool haveWindowMin_ = false;
};

#endif
