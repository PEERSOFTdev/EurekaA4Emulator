#ifndef EUREKA_TCP_LINK_H
#define EUREKA_TCP_LINK_H

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "serial_link.h"

// A null-modem cable between two emulators over TCP (ea4-7zw.4): one waits on
// a port, the other connects to it.  No server in between, so it works on one
// computer, on a home network and over a VPN such as Tailscale.
//
// The machine calls the SerialLink half from its own thread; the socket lives
// on a thread of this class, and the two meet in two queues under one lock.
// Each machine runs in real time, so nothing here paces anything -- the sending
// machine already spaces its characters by the character time and the
// receiving one takes them when its receiver is free.
//
// On the wire every byte is itself except FFh, which starts a pair: FF FF is
// the byte FFh, FF 00 and FF 01 are our RTS dropped and asserted.  RTS rides in
// the same stream as the data so it can never overtake a byte sent before it.
class TcpLink : public SerialLink {
 public:
  enum class State { kIdle, kListening, kConnecting, kConnected };
  enum class Event {
    kConnected,
    // The far end went away.  Not reported for Close(), which the caller knows.
    kLost,
    // Connect() gave up; the detail says why.
    kFailed,
  };
  // Called on the link's own thread.  It must not call back into the link --
  // Close() joins that very thread.
  using Listener = std::function<void(Event, const std::wstring& detail)>;

  // 4161 is not assigned to anything that would be running on a home PC.
  static constexpr uint16_t kDefaultPort = 4161;

  TcpLink();
  ~TcpLink() override;
  TcpLink(const TcpLink&) = delete;
  TcpLink& operator=(const TcpLink&) = delete;

  // Set before Listen or Connect.
  void SetListener(Listener listener) { listener_ = std::move(listener); }

  // Waits for the other emulator on `port` (0 = any free one, for tests).  The
  // port is taken here and now, so a port already in use is refused at once
  // and not later from another thread.  A listener that loses its peer goes
  // back to waiting -- the cable stays plugged into our socket.
  bool Listen(uint16_t port, std::wstring& error);
  // Connects in the background; the outcome arrives as kConnected or kFailed.
  // A connection that is lost is not retried.
  void Connect(const std::wstring& host, uint16_t port);
  // Unplugs the cable.  Blocks while a name lookup is in progress.
  void Close();

  State state() const { return state_.load(); }
  // The port actually listened on; useful after Listen(0).
  uint16_t port() const { return port_.load(); }

  // SerialLink.  With nothing at the other end, CTS is not asserted and what
  // is transmitted is lost -- the same as an unplugged socket.
  bool ClearToSend() override;
  void SetRequestToSend(bool asserted) override;
  void Transmit(uint8_t character) override;
  std::optional<uint8_t> Receive() override;

 private:
  void ListenLoop();
  void ConnectLoop(std::wstring host, uint16_t port);
  // Runs one connection until it ends; true when the far end went away, false
  // when Close() asked for it.
  bool Serve(uintptr_t socket);
  void Decode(const char* data, std::size_t size);
  void Queue(uint8_t first, std::optional<uint8_t> second);
  void Notify(Event event, const std::wstring& detail = {});

  Listener listener_;
  std::thread worker_;
  // WSAEVENT handles; void* so that this header does not drag in winsock2.h.
  void* stop_ = nullptr;
  void* wake_ = nullptr;
  void* accept_ = nullptr;
  uintptr_t listenSocket_;
  bool winsock_ = false;

  std::atomic<State> state_{State::kIdle};
  std::atomic<uint16_t> port_{0};
  std::atomic<bool> peerRts_{false};
  std::atomic<bool> ourRts_{false};
  // Decoder state, touched only by the worker.
  bool escape_ = false;

  std::mutex lock_;
  std::deque<uint8_t> inbox_;
  std::string outbox_;
};

// "host", "host:port", "[v6]:port", "[v6]" or a bare IPv6 address.  The port
// must be 1 to 65535; without one, `defaultPort`.  False for anything else.
bool ParseTcpAddress(const std::wstring& text, uint16_t defaultPort,
                     std::wstring& host, uint16_t& port);
// A port alone, 1 to 65535.
bool ParseTcpPort(const std::wstring& text, uint16_t& port);

#endif
