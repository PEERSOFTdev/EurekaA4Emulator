// The part of the updates that talks to the network and to the user: asking
// GitHub for the newest release, downloading and verifying the EXE, putting it
// in place of the running one and starting it (epic ea4-hg9).  The rules --
// what to offer, how to read GitHub's answer -- are in update.h, where a test
// can hold them without a connection.
//
// Replacing the EXE works while it runs: Windows refuses to overwrite a
// running image but lets it be renamed.  So the new file is written beside it
// as .new, the running one becomes .old, the new one takes its name, and the
// .old is removed by the next start.  Nothing is replaced until the download
// matched the hash GitHub publishes for it.
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

namespace updater {

struct Latest {
  enum class Kind { kFound, kNoRelease, kFailed };
  Kind kind = Kind::kFailed;
  std::wstring tag;    // kFound: "v2026.9.2"
  std::wstring error;  // kFailed: why, in words for the user
};

// Asks github.com which release is the newest.  timeoutMs caps each network
// phase; 0 leaves WinHTTP's defaults, for a check the user asked for and can
// wait on.  Blocking.
Latest FetchLatest(DWORD timeoutMs);

// The automatic check's cap on the whole question, DNS included, which
// WinHTTP's own timeouts do not cover: past this the start goes on and the
// answer, if it ever comes, is dropped.  A slow network must not hold up the
// machine the user started (owner's decision, 29. 9. 2026).
Latest FetchLatestWithin(DWORD milliseconds);

enum class Choice { kUpdate, kLater, kSkip };

// "Je k dispozícii verzia ...", with Aktualizovať, Neskôr and Preskočiť túto
// verziu.  Closing the dialog is Neskôr.
Choice AskToUpdate(HWND owner, const std::wstring& tag);

enum class Installed { kRestarted, kInstalled, kCancelled, kFailed };

// Downloads the release's EXE and SHA256SUMS.txt behind a progress dialog the
// user can cancel, checks the hash and puts the EXE in place of this one.
// With `restart` it also starts the new EXE -- before the dialog closes, so
// that the new window gets the focus (see Finish in updater.cpp) -- and
// kRestarted means this process should leave.  kInstalled is the new EXE in
// place and not started: asked for, or starting it failed, which has been
// said.  On kFailed a message has been shown too, including the offer to open
// the release page when the EXE's folder cannot be written to.
Installed DownloadAndInstall(HWND owner, const std::wstring& tag, bool restart);

// Starts the EXE now in place, with this run's command line.  For a restart
// that has to wait -- the machine running (ea4-hg9.5) -- so this process may
// no longer have the focus to hand over by then.
bool Restart(std::wstring& error);

// The .old a previous update left behind.  Called at every start; failing is
// fine -- the old process may still be closing, and the next start tries again.
void RemoveLeftover();

// Local date "YYYY-MM-DD", the form Settings keeps the last check in.
std::wstring Today();

}  // namespace updater
