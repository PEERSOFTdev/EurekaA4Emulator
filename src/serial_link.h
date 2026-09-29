#ifndef EUREKA_SERIAL_LINK_H
#define EUREKA_SERIAL_LINK_H

#include <cstdint>
#include <optional>

// Whatever is plugged into the RS-232 socket at the back -- ASCI channel 1,
// the port both the printer and Komunikace use (HANDOFF 6.3, 6.53).  The
// machine sees only the wires: characters both ways, its own RTS going out and
// the far end's answer coming back on CTS.  It does not know whether that is a
// second machine in the same process, a socket or a file.
//
// Every call comes from the thread that runs the machine.  An end that is fed
// from another thread has to do its own locking.
class SerialLink {
 public:
  virtual ~SerialLink() = default;

  // The far end holding our CTS asserted.  Printing and sending both wait for
  // it at 184F2 and give up with "neni pripravena" / "neni odezva".
  virtual bool ClearToSend() = 0;
  // Our RTS1 (B0h bit 7), true when asserted.  The firmware only drops it when
  // its receive buffer is nearly full (1860D, HANDOFF 6.4).
  virtual void SetRequestToSend(bool asserted) = 0;
  // A whole character has left the transmit shift register.
  virtual void Transmit(uint8_t character) = 0;
  // The next character that has arrived, if any.  Asked only when the receiver
  // can take one, so an end never has to drop anything.
  virtual std::optional<uint8_t> Receive() = 0;
};

#endif
