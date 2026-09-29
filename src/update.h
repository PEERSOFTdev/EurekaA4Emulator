// Updates from the project's GitHub releases (epic ea4-hg9).
//
// The check runs before the machine starts, so taking an update is only
// swapping the EXE and starting it again: there is no RAM to save and no
// diskette to ask about, because nothing is running yet.
//
// This header is the part that needs no network: reading what GitHub answered
// and deciding whether to offer anything.  It is kept apart from the download
// so update_test can hold the rules without WinHTTP and without a connection.
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "version.h"

namespace update {

// Where releases are published.  The fork contributions come from is not it:
// releases are made from lpintes/EurekaA4Emulator only (vydanie.yml).
inline constexpr wchar_t kHost[] = L"github.com";
inline constexpr wchar_t kLatestPath[] =
    L"/lpintes/EurekaA4Emulator/releases/latest";
inline constexpr wchar_t kReleasePage[] =
    L"https://github.com/lpintes/EurekaA4Emulator/releases/latest";
// Asset names without a version in them, so a download path can be built from
// the tag alone.  vydanie.yml publishes exactly these.
inline constexpr wchar_t kExeAsset[] = L"EurekaA4Emulator.exe";
inline constexpr wchar_t kSumsAsset[] = L"SHA256SUMS.txt";

// github.com/.../releases/latest answers with a redirect to the newest
// release's page, ".../releases/tag/v2026.9.2".  Reading the tag from that
// Location header needs no API, no JSON and no rate limit.  Anything that is
// not a release tag -- with no release yet GitHub redirects to ".../releases"
// -- yields nothing.
std::optional<std::wstring> TagFromLocation(std::wstring_view location);

// The SHA-256 that SHA256SUMS.txt gives for one file, in lower case, or
// nothing when the file is not listed or its line is malformed.  The format is
// sha256sum's: "<64 hex>  <name>", or "<64 hex> *<name>" in binary mode.
std::optional<std::string> HashFor(std::string_view sums, std::string_view file);

// Whether the automatic check at start-up should offer latestTag.  Only a
// release build offers anything (a development build has no number and must
// not replace itself with an older EXE), only a newer version, and never the
// one the user said to skip -- though a release newer than that one is
// offered again.
bool ShouldOffer(std::optional<version::Number> current,
                 std::wstring_view latestTag, std::wstring_view skipped);

// Once a day: the check is due unless it already ran today.  Dates are
// "YYYY-MM-DD" in local time, compared as they are written.
bool CheckDue(std::wstring_view lastCheck, std::wstring_view today);

}  // namespace update
