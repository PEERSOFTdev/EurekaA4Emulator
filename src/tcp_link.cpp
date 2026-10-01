#include "tcp_link.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

namespace {

constexpr uint8_t kEscape = 0xff;
constexpr uint8_t kRtsDropped = 0x00;
constexpr uint8_t kRtsAsserted = 0x01;
// Per address tried.  Windows itself gives up on a refused port after about
// two seconds; this is for an address where nothing answers at all.
constexpr DWORD kConnectTimeoutMs = 10000;

// The system's own wording, in the user's language.
std::wstring ErrorText(int code) {
  wchar_t* buffer = nullptr;
  const DWORD length = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, static_cast<DWORD>(code), 0, reinterpret_cast<wchar_t*>(&buffer), 0,
      nullptr);
  std::wstring text = length != 0 ? std::wstring(buffer, length)
                                  : L"chyba " + std::to_wstring(code);
  if (buffer != nullptr) LocalFree(buffer);
  while (!text.empty() &&
         (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' '))
    text.pop_back();
  return text;
}

bool Signaled(void* event) {
  return WSAWaitForMultipleEvents(1, &event, FALSE, 0, FALSE) == WSA_WAIT_EVENT_0;
}

}  // namespace

TcpLink::TcpLink() : listenSocket_(INVALID_SOCKET) {
  WSADATA data;
  winsock_ = WSAStartup(MAKEWORD(2, 2), &data) == 0;
  stop_ = WSACreateEvent();
  wake_ = WSACreateEvent();
  accept_ = WSACreateEvent();
}

TcpLink::~TcpLink() {
  Close();
  WSACloseEvent(stop_);
  WSACloseEvent(wake_);
  WSACloseEvent(accept_);
  if (winsock_) WSACleanup();
}

bool TcpLink::Listen(uint16_t port, std::wstring& error) {
  Close();
  // One socket for IPv4 and IPv6 both; plain IPv4 where IPv6 is switched off.
  SOCKET listening = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
  sockaddr_storage address{};
  int addressSize = 0;
  if (listening != INVALID_SOCKET) {
    DWORD off = 0;
    setsockopt(listening, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&off),
               sizeof off);
    auto& v6 = reinterpret_cast<sockaddr_in6&>(address);
    v6.sin6_family = AF_INET6;
    v6.sin6_port = htons(port);
    v6.sin6_addr = in6addr_any;
    addressSize = sizeof v6;
  } else {
    listening = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    auto& v4 = reinterpret_cast<sockaddr_in&>(address);
    v4.sin_family = AF_INET;
    v4.sin_port = htons(port);
    v4.sin_addr.s_addr = htonl(INADDR_ANY);
    addressSize = sizeof v4;
  }
  if (listening == INVALID_SOCKET) {
    error = ErrorText(WSAGetLastError());
    return false;
  }
  // Without this a second emulator on the same computer could share the port
  // and the connection would land in whichever of them Windows picks.
  BOOL on = TRUE;
  setsockopt(listening, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&on),
             sizeof on);
  if (bind(listening, reinterpret_cast<sockaddr*>(&address), addressSize) != 0 ||
      listen(listening, 1) != 0 ||
      getsockname(listening, reinterpret_cast<sockaddr*>(&address), &addressSize) != 0 ||
      WSAEventSelect(listening, accept_, FD_ACCEPT) != 0) {
    error = ErrorText(WSAGetLastError());
    closesocket(listening);
    return false;
  }
  // The port sits at the same offset in sockaddr_in and sockaddr_in6.
  port_ = ntohs(reinterpret_cast<sockaddr_in&>(address).sin_port);
  listenSocket_ = listening;
  state_ = State::kListening;
  worker_ = std::thread(&TcpLink::ListenLoop, this);
  return true;
}

void TcpLink::Connect(const std::wstring& host, uint16_t port) {
  Close();
  state_ = State::kConnecting;
  worker_ = std::thread(&TcpLink::ConnectLoop, this, host, port);
}

void TcpLink::Close() {
  if (worker_.joinable()) {
    WSASetEvent(stop_);
    worker_.join();
  }
  if (listenSocket_ != INVALID_SOCKET) {
    closesocket(listenSocket_);
    listenSocket_ = INVALID_SOCKET;
  }
  WSAResetEvent(stop_);
  WSAResetEvent(accept_);
  state_ = State::kIdle;
  port_ = 0;
  peerRts_ = false;
  std::lock_guard guard(lock_);
  inbox_.clear();
  outbox_.clear();
}

bool TcpLink::ClearToSend() {
  return state_ == State::kConnected && peerRts_;
}

void TcpLink::SetRequestToSend(bool asserted) {
  if (ourRts_.exchange(asserted) == asserted) return;
  // Serve() sends the current state when a connection starts, so a change
  // while there is none is not lost.
  if (state_ == State::kConnected)
    Queue(kEscape, asserted ? kRtsAsserted : kRtsDropped);
}

void TcpLink::Transmit(uint8_t character) {
  if (state_ != State::kConnected) return;
  if (character == kEscape) Queue(kEscape, kEscape);
  else Queue(character, std::nullopt);
}

std::optional<uint8_t> TcpLink::Receive() {
  std::lock_guard guard(lock_);
  if (inbox_.empty()) return std::nullopt;
  const uint8_t character = inbox_.front();
  inbox_.pop_front();
  return character;
}

void TcpLink::Queue(uint8_t first, std::optional<uint8_t> second) {
  {
    std::lock_guard guard(lock_);
    outbox_.push_back(static_cast<char>(first));
    if (second) outbox_.push_back(static_cast<char>(*second));
  }
  WSASetEvent(wake_);
}

void TcpLink::Notify(Event event, const std::wstring& detail) {
  if (listener_) listener_(event, detail);
}

void TcpLink::ListenLoop() {
  const SOCKET listening = listenSocket_;
  for (;;) {
    WSAEVENT events[] = {stop_, accept_};
    WSAWaitForMultipleEvents(2, events, FALSE, WSA_INFINITE, FALSE);
    if (Signaled(stop_)) return;
    WSANETWORKEVENTS happened{};
    WSAEnumNetworkEvents(listening, accept_, &happened);
    const SOCKET peer = accept(listening, nullptr, nullptr);
    if (peer == INVALID_SOCKET) continue;
    if (!Serve(peer)) return;
    state_ = State::kListening;
    Notify(Event::kLost);
  }
}

void TcpLink::ConnectLoop(std::wstring host, uint16_t port) {
  ADDRINFOW hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  ADDRINFOW* found = nullptr;
  const int resolved = GetAddrInfoW(host.c_str(), std::to_wstring(port).c_str(), &hints, &found);
  if (resolved != 0) {
    state_ = State::kIdle;
    Notify(Event::kFailed, ErrorText(resolved));
    return;
  }
  WSAEVENT connecting = WSACreateEvent();
  SOCKET connected = INVALID_SOCKET;
  int lastError = WSAETIMEDOUT;
  bool stopped = false;
  for (ADDRINFOW* candidate = found; candidate != nullptr && !stopped;
       candidate = candidate->ai_next) {
    const SOCKET attempt =
        socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
    if (attempt == INVALID_SOCKET) {
      lastError = WSAGetLastError();
      continue;
    }
    WSAResetEvent(connecting);
    WSAEventSelect(attempt, connecting, FD_CONNECT);
    if (connect(attempt, candidate->ai_addr, static_cast<int>(candidate->ai_addrlen)) == 0) {
      connected = attempt;
      break;
    }
    if (WSAGetLastError() != WSAEWOULDBLOCK) {
      lastError = WSAGetLastError();
      closesocket(attempt);
      continue;
    }
    WSAEVENT events[] = {stop_, connecting};
    const DWORD which =
        WSAWaitForMultipleEvents(2, events, FALSE, kConnectTimeoutMs, FALSE);
    if (which == WSA_WAIT_EVENT_0) {
      stopped = true;
    } else if (which == WSA_WAIT_EVENT_0 + 1) {
      WSANETWORKEVENTS happened{};
      WSAEnumNetworkEvents(attempt, connecting, &happened);
      if ((happened.lNetworkEvents & FD_CONNECT) != 0 &&
          happened.iErrorCode[FD_CONNECT_BIT] == 0) {
        connected = attempt;
        break;
      }
      lastError = happened.iErrorCode[FD_CONNECT_BIT];
    } else {
      lastError = WSAETIMEDOUT;
    }
    closesocket(attempt);
  }
  FreeAddrInfoW(found);
  WSACloseEvent(connecting);
  if (stopped) return;
  if (connected == INVALID_SOCKET) {
    state_ = State::kIdle;
    Notify(Event::kFailed, ErrorText(lastError));
    return;
  }
  if (!Serve(connected)) return;
  state_ = State::kIdle;
  Notify(Event::kLost);
}

bool TcpLink::Serve(uintptr_t handle) {
  const SOCKET peer = handle;
  // Komunikace waits for each ACK before the next block; Nagle would hold
  // every lone ACK back for the delayed ACK of the other side.
  BOOL on = TRUE;
  setsockopt(peer, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&on), sizeof on);
  WSAEVENT traffic = WSACreateEvent();
  WSAEventSelect(peer, traffic, FD_READ | FD_WRITE | FD_CLOSE);
  {
    // Whatever was left from an earlier connection belongs to that one.
    std::lock_guard guard(lock_);
    inbox_.clear();
    outbox_.clear();
    outbox_.push_back(static_cast<char>(kEscape));
    outbox_.push_back(static_cast<char>(ourRts_ ? kRtsAsserted : kRtsDropped));
  }
  escape_ = false;
  peerRts_ = false;
  state_ = State::kConnected;
  Notify(Event::kConnected);

  const bool listening = listenSocket_ != INVALID_SOCKET;
  std::string pending;
  bool lost = false;
  while (!lost) {
    // Reset before draining: a Transmit after the drain sets it again, so no
    // wake-up falls between the two.
    WSAResetEvent(wake_);
    {
      std::lock_guard guard(lock_);
      pending += outbox_;
      outbox_.clear();
    }
    while (!pending.empty()) {
      const int sent = send(peer, pending.data(), static_cast<int>(pending.size()), 0);
      if (sent == SOCKET_ERROR) {
        // A full send buffer reports FD_WRITE once it drains.
        if (WSAGetLastError() != WSAEWOULDBLOCK) lost = true;
        break;
      }
      pending.erase(0, static_cast<std::size_t>(sent));
    }
    if (lost) break;

    WSAEVENT events[] = {stop_, wake_, traffic, accept_};
    WSAWaitForMultipleEvents(listening ? 4 : 3, events, FALSE, WSA_INFINITE, FALSE);
    if (Signaled(stop_)) break;
    WSANETWORKEVENTS happened{};
    WSAEnumNetworkEvents(peer, traffic, &happened);
    if ((happened.lNetworkEvents & (FD_READ | FD_CLOSE)) != 0) {
      char buffer[4096];
      for (;;) {
        const int got = recv(peer, buffer, sizeof buffer, 0);
        if (got > 0) {
          Decode(buffer, static_cast<std::size_t>(got));
          continue;
        }
        if (got == 0 || WSAGetLastError() != WSAEWOULDBLOCK) lost = true;
        break;
      }
    }
    if ((happened.lNetworkEvents & FD_CLOSE) != 0) lost = true;
    if (listening) {
      // A third emulator knocking while the cable is in use is turned away
      // rather than left hanging in the backlog until this one ends.
      WSANETWORKEVENTS knock{};
      WSAEnumNetworkEvents(listenSocket_, accept_, &knock);
      if ((knock.lNetworkEvents & FD_ACCEPT) != 0) {
        const SOCKET extra = accept(listenSocket_, nullptr, nullptr);
        if (extra != INVALID_SOCKET) closesocket(extra);
      }
    }
  }
  closesocket(peer);
  WSACloseEvent(traffic);
  peerRts_ = false;
  return lost;
}

void TcpLink::Decode(const char* data, std::size_t size) {
  std::lock_guard guard(lock_);
  for (std::size_t i = 0; i < size; ++i) {
    const uint8_t byte = static_cast<uint8_t>(data[i]);
    if (!escape_) {
      if (byte == kEscape) escape_ = true;
      else inbox_.push_back(byte);
      continue;
    }
    escape_ = false;
    if (byte == kEscape) inbox_.push_back(byte);
    else if (byte == kRtsAsserted) peerRts_ = true;
    else if (byte == kRtsDropped) peerRts_ = false;
  }
}

bool ParseTcpPort(const std::wstring& text, uint16_t& port) {
  if (text.empty() || text.size() > 5) return false;
  unsigned value = 0;
  for (const wchar_t ch : text) {
    if (ch < L'0' || ch > L'9') return false;
    value = value * 10 + static_cast<unsigned>(ch - L'0');
  }
  if (value == 0 || value > 65535) return false;
  port = static_cast<uint16_t>(value);
  return true;
}

bool ParseTcpAddress(const std::wstring& raw, uint16_t defaultPort, std::wstring& host,
                     uint16_t& port) {
  const std::size_t first = raw.find_first_not_of(L" \t");
  if (first == std::wstring::npos) return false;
  const std::wstring text = raw.substr(first, raw.find_last_not_of(L" \t") - first + 1);
  std::wstring name;
  std::wstring portText;
  bool hasPort = false;
  if (text[0] == L'[') {
    const std::size_t close = text.find(L']');
    if (close == std::wstring::npos) return false;
    name = text.substr(1, close - 1);
    const std::wstring rest = text.substr(close + 1);
    if (!rest.empty()) {
      if (rest[0] != L':') return false;
      portText = rest.substr(1);
      hasPort = true;
    }
  } else {
    const std::size_t colon = text.find(L':');
    if (colon != std::wstring::npos && text.find(L':', colon + 1) == std::wstring::npos) {
      name = text.substr(0, colon);
      portText = text.substr(colon + 1);
      hasPort = true;
    } else {
      // No colon, or several: a bare IPv6 address takes no port.
      name = text;
    }
  }
  if (name.empty() || name.find_first_of(L" \t[]") != std::wstring::npos) return false;
  uint16_t value = defaultPort;
  if (hasPort && !ParseTcpPort(portText, value)) return false;
  host = name;
  port = value;
  return true;
}
