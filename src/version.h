// Which version this EXE is, and how two versions compare.
//
// The version is not written in any source file.  The Makefile asks git for
// the tag the build stands on (git describe) and generates build/ea4_version.h
// from it, so the tag is the only place a release number lives.  A second copy
// in a file would drift from the tag the day somebody forgot to bump it.
//
// A release is year.month.serial, "2026.9.1", tagged "v2026.9.1"; the serial
// starts again at 1 every month.  Anything built off a tag carries git's
// suffix ("2026.9.1-5-gabc1234", "-dirty") and is a development build: it
// has no number to compare, so it never takes itself for older than a release
// and never offers to replace itself with one.
#pragma once

#include <compare>
#include <optional>
#include <string_view>

namespace version {

struct Number {
  int year = 0;
  int month = 0;
  int serial = 0;
  // Members in this order, so the defaulted comparison is year first.  Compared
  // as numbers, not as text: as text "2026.9.5" would beat "2026.10.1".
  auto operator<=>(const Number&) const = default;
};

// The version as the user is told it: "2026.9.1", or with git's suffix for a
// development build.
std::wstring_view Current();

// The release number of this EXE, or nothing for a development build.
std::optional<Number> CurrentNumber();

// "2026.9.1" or "v2026.9.1" and nothing else: exactly three decimal numbers,
// no suffix.  Everything else is not a release and yields nothing.
std::optional<Number> Parse(std::wstring_view text);

}  // namespace version
