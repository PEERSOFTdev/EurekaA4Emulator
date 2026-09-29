#include "updater.h"

#include <bcrypt.h>
#include <commctrl.h>
#include <shellapi.h>
#include <winhttp.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "update.h"
#include "version.h"

namespace fs = std::filesystem;

namespace updater {

namespace {

class Internet {
 public:
  explicit Internet(HINTERNET handle = nullptr) : handle_(handle) {}
  ~Internet() {
    if (handle_) WinHttpCloseHandle(handle_);
  }
  Internet(const Internet&) = delete;
  Internet& operator=(const Internet&) = delete;
  HINTERNET get() const { return handle_; }
  explicit operator bool() const { return handle_ != nullptr; }

 private:
  HINTERNET handle_;
};

// What went wrong, in words.  The common cases are named: the raw system text
// for them is English and says nothing a user can act on.
std::wstring NetworkError(DWORD code) {
  switch (code) {
    case ERROR_WINHTTP_NAME_NOT_RESOLVED:
      return L"server github.com sa nenašiel. Ste pripojený na internet?";
    case ERROR_WINHTTP_TIMEOUT:
      return L"server neodpovedal včas.";
    case ERROR_WINHTTP_CANNOT_CONNECT:
    case ERROR_WINHTTP_CONNECTION_ERROR:
      return L"spojenie so serverom github.com zlyhalo.";
    case ERROR_WINHTTP_SECURE_FAILURE:
      return L"zabezpečené spojenie so serverom sa nepodarilo overiť.";
    default:
      return L"chyba siete " + std::to_wstring(code) + L".";
  }
}

struct Response {
  DWORD status = 0;
  std::wstring location;
  std::string body;
};

// Shared by a download and whoever watches it: the progress dialog reads the
// counters on its timer, and sets `cancel` when the user gives up.
struct Progress {
  std::atomic<std::uint64_t> done{0};
  std::atomic<std::uint64_t> total{0};
  std::atomic<bool> cancel{false};
};

// One GET to github.com.  Redirects are followed for downloads -- the asset
// URL redirects to GitHub's storage host -- and not for releases/latest, whose
// redirect *is* the answer.
bool Get(const std::wstring& path, bool followRedirects, DWORD timeoutMs,
         Progress* progress, Response& out, std::wstring& error) {
  const std::wstring agent =
      L"EurekaA4Emulator/" + std::wstring(version::Current());
  Internet session(WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
  if (!session) {
    error = NetworkError(GetLastError());
    return false;
  }
  if (timeoutMs != 0) {
    const int ms = static_cast<int>(timeoutMs);
    WinHttpSetTimeouts(session.get(), ms, ms, ms, ms);
  }
  Internet connection(WinHttpConnect(session.get(), update::kHost,
                                     INTERNET_DEFAULT_HTTPS_PORT, 0));
  if (!connection) {
    error = NetworkError(GetLastError());
    return false;
  }
  Internet request(WinHttpOpenRequest(connection.get(), L"GET", path.c_str(),
                                      nullptr, WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES,
                                      WINHTTP_FLAG_SECURE));
  if (!request) {
    error = NetworkError(GetLastError());
    return false;
  }
  if (!followRedirects) {
    DWORD option = WINHTTP_DISABLE_REDIRECTS;
    WinHttpSetOption(request.get(), WINHTTP_OPTION_DISABLE_FEATURE, &option,
                     sizeof(option));
  }
  if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                          WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
      !WinHttpReceiveResponse(request.get(), nullptr)) {
    error = NetworkError(GetLastError());
    return false;
  }

  DWORD size = sizeof(out.status);
  WinHttpQueryHeaders(request.get(),
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &out.status, &size,
                      WINHTTP_NO_HEADER_INDEX);
  size = 0;
  WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_LOCATION,
                      WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size,
                      WINHTTP_NO_HEADER_INDEX);
  if (size > 0) {
    std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1);
    if (WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_LOCATION,
                            WINHTTP_HEADER_NAME_BY_INDEX, buffer.data(), &size,
                            WINHTTP_NO_HEADER_INDEX))
      out.location.assign(buffer.data(), size / sizeof(wchar_t));
  }
  if (!followRedirects) return true;

  if (progress) {
    DWORD length = 0;
    size = sizeof(length);
    if (WinHttpQueryHeaders(request.get(),
                            WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &length, &size,
                            WINHTTP_NO_HEADER_INDEX))
      progress->total = length;
  }
  for (;;) {
    if (progress && progress->cancel) return false;
    DWORD available = 0;
    if (!WinHttpQueryDataAvailable(request.get(), &available)) {
      error = NetworkError(GetLastError());
      return false;
    }
    if (available == 0) break;
    const std::size_t at = out.body.size();
    out.body.resize(at + available);
    DWORD read = 0;
    if (!WinHttpReadData(request.get(), out.body.data() + at, available, &read)) {
      error = NetworkError(GetLastError());
      return false;
    }
    out.body.resize(at + read);
    if (progress) progress->done = out.body.size();
  }
  return true;
}

// Lower-case hex SHA-256, or nothing if BCrypt refused.
std::optional<std::string> Sha256(const std::string& data) {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
                                  0) < 0)
    return std::nullopt;
  unsigned char digest[32] = {};
  BCRYPT_HASH_HANDLE hash = nullptr;
  bool ok = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
  ok = ok && BCryptHashData(hash, reinterpret_cast<PUCHAR>(
                                      const_cast<char*>(data.data())),
                            static_cast<ULONG>(data.size()), 0) >= 0;
  ok = ok && BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
  if (hash) BCryptDestroyHash(hash);
  BCryptCloseAlgorithmProvider(algorithm, 0);
  if (!ok) return std::nullopt;
  static const char kHex[] = "0123456789abcdef";
  std::string text;
  for (unsigned char byte : digest) {
    text += kHex[byte >> 4];
    text += kHex[byte & 15];
  }
  return text;
}

fs::path ThisExe() {
  std::wstring buffer(MAX_PATH, L'\0');
  for (;;) {
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (length == 0) return {};
    if (length < buffer.size()) {
      buffer.resize(length);
      return buffer;
    }
    buffer.resize(buffer.size() * 2);
  }
}

std::wstring Bare(const std::wstring& tag) {
  return tag.starts_with(L'v') ? tag.substr(1) : tag;
}

std::wstring AssetPath(const std::wstring& tag, const wchar_t* asset) {
  // kLatestPath is ".../releases/latest"; the download lives beside it.
  std::wstring path = update::kLatestPath;
  path.resize(path.size() - std::wcslen(L"latest"));
  return path + L"download/" + tag + L"/" + asset;
}

void OpenReleasePage() {
  ShellExecuteW(nullptr, L"open", update::kReleasePage, nullptr, nullptr,
                SW_SHOWNORMAL);
}

// The download runs on its own thread and the dialog only watches it: a
// TaskDialog runs its own message loop, and the network must not stall it.
// The state is shared, so a cancelled download can finish or fail on its own
// time without touching anything that is gone.
struct Download {
  std::wstring tag;
  fs::path target;  // the EXE to replace
  bool restart = false;
  Progress progress;
  std::atomic<bool> finished{false};
  bool ok = false;  // written before `finished`, read after it
  std::string exe;
  std::string sums;
  std::wstring error;
  // Settled on the dialog's thread once the download is in, before the
  // dialog closes; see Finish.
  enum class Outcome { kPending, kInstalled, kRestarted, kNotWritable, kFailed };
  Outcome outcome = Outcome::kPending;
  std::wstring restartError;
};

enum class Written { kOk, kNotWritable, kFailed };
Written Replace(const fs::path& target, const std::string& bytes,
                std::wstring& error);
bool StartNew(const fs::path& exe, std::wstring& error);

// Checks the download, swaps the EXE and, when asked, starts the new one --
// all while the progress dialog is still up.  That is the point of doing it
// here and not after the dialog returns: Windows lets a new window take the
// focus only if whoever started it had the focus at that moment, and once the
// dialog is gone this process has no window at all.  Started afterwards, the
// new emulator came up behind the terminal and had to be found with Alt+Tab
// (owner's test, 29. 9. 2026).
void Finish(Download& job) {
  if (!job.ok) {
    job.outcome = Download::Outcome::kFailed;
    return;
  }
  // The asset name is ASCII, so narrowing it is a copy.
  const std::wstring_view wide = update::kExeAsset;
  const std::string name(wide.begin(), wide.end());
  const std::optional<std::string> expected = update::HashFor(job.sums, name);
  const std::optional<std::string> actual = Sha256(job.exe);
  if (!expected) {
    job.error = L"k vydaniu chýba kontrolný súčet.";
    job.outcome = Download::Outcome::kFailed;
    return;
  }
  if (!actual || *actual != *expected) {
    job.error = L"stiahnutý súbor nesúhlasí s kontrolným súčtom.";
    job.outcome = Download::Outcome::kFailed;
    return;
  }
  switch (Replace(job.target, job.exe, job.error)) {
    case Written::kOk:
      break;
    case Written::kNotWritable:
      job.outcome = Download::Outcome::kNotWritable;
      return;
    case Written::kFailed:
      job.outcome = Download::Outcome::kFailed;
      return;
  }
  job.outcome = job.restart && StartNew(job.target, job.restartError)
                    ? Download::Outcome::kRestarted
                    : Download::Outcome::kInstalled;
}

void RunDownload(const std::shared_ptr<Download>& job) {
  Response exe;
  Response sums;
  std::wstring error;
  bool ok = Get(AssetPath(job->tag, update::kSumsAsset), true, 0, nullptr, sums,
                error) &&
            Get(AssetPath(job->tag, update::kExeAsset), true, 0, &job->progress,
                exe, error);
  if (ok && (sums.status != 200 || exe.status != 200)) {
    error = L"server vrátil kód " +
            std::to_wstring(exe.status != 200 ? exe.status : sums.status) + L".";
    ok = false;
  }
  job->exe = std::move(exe.body);
  job->sums = std::move(sums.body);
  job->error = std::move(error);
  job->ok = ok;
  job->finished = true;
}

HRESULT CALLBACK ProgressCallback(HWND dialog, UINT notification, WPARAM wParam,
                                  LPARAM, LONG_PTR data) {
  auto* job = reinterpret_cast<Download*>(data);
  switch (notification) {
    case TDN_TIMER: {
      const std::uint64_t total = job->progress.total;
      if (total > 0) {
        const auto percent = static_cast<WPARAM>(job->progress.done * 100 / total);
        SendMessageW(dialog, TDM_SET_PROGRESS_BAR_POS, percent, 0);
      }
      if (job->finished && job->outcome == Download::Outcome::kPending) {
        if (!job->progress.cancel) Finish(*job);
        SendMessageW(dialog, TDM_CLICK_BUTTON, IDCANCEL, 0);
      }
      break;
    }
    case TDN_BUTTON_CLICKED:
      // The same button closes the dialog when the download ends; only a click
      // before that is the user giving up.
      if (wParam == IDCANCEL && !job->finished) job->progress.cancel = true;
      break;
    default:
      break;
  }
  return S_OK;
}

// Writes the new EXE beside the running one and swaps the two names.  The
// running image can be renamed but not overwritten; if the second rename
// fails the first is undone, so there is never no EXE at the old path.
Written Replace(const fs::path& target, const std::string& bytes,
                std::wstring& error) {
  fs::path fresh = target;
  fresh += L".new";
  fs::path old = target;
  old += L".old";
  const HANDLE file = CreateFileW(fresh.c_str(), GENERIC_WRITE, 0, nullptr,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    const DWORD code = GetLastError();
    if (code == ERROR_ACCESS_DENIED) return Written::kNotWritable;
    error = L"nový súbor sa nedá zapísať (chyba " + std::to_wstring(code) + L").";
    return Written::kFailed;
  }
  DWORD written = 0;
  const bool wrote = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()),
                               &written, nullptr) &&
                     written == bytes.size() && FlushFileBuffers(file);
  CloseHandle(file);
  if (!wrote) {
    DeleteFileW(fresh.c_str());
    error = L"nový súbor sa nedá zapísať celý.";
    return Written::kFailed;
  }
  DeleteFileW(old.c_str());
  if (!MoveFileExW(target.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING)) {
    const DWORD code = GetLastError();
    DeleteFileW(fresh.c_str());
    if (code == ERROR_ACCESS_DENIED) return Written::kNotWritable;
    error = L"doterajší súbor sa nedá odsunúť (chyba " + std::to_wstring(code) +
            L").";
    return Written::kFailed;
  }
  if (!MoveFileExW(fresh.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING)) {
    const DWORD code = GetLastError();
    MoveFileExW(old.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
    DeleteFileW(fresh.c_str());
    error = L"nový súbor sa nedá dať na miesto (chyba " + std::to_wstring(code) +
            L").";
    return Written::kFailed;
  }
  return Written::kOk;
}

// Starts exe with this run's command line, so --rom and the rest carry over.
// The new process may take the focus: we hand it our right to, while we still
// have it (see Finish).
bool StartNew(const fs::path& exe, std::wstring& error) {
  std::wstring commandLine = GetCommandLineW();
  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process = {};
  if (!CreateProcessW(exe.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
                      0, nullptr, nullptr, &startup, &process)) {
    error = L"novú verziu sa nepodarilo spustiť (chyba " +
            std::to_wstring(GetLastError()) + L"). Spustite ju sami.";
    return false;
  }
  AllowSetForegroundWindow(process.dwProcessId);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return true;
}

}  // namespace

Latest FetchLatest(DWORD timeoutMs) {
  Latest result;
  Response response;
  if (!Get(update::kLatestPath, false, timeoutMs, nullptr, response,
           result.error))
    return result;
  if (response.status < 300 || response.status >= 400) {
    // 404 is what a repository with no release at all would give if GitHub
    // ever stopped redirecting; neither is an error worth a word.
    if (response.status == 404) result.kind = Latest::Kind::kNoRelease;
    else result.error = L"server vrátil kód " + std::to_wstring(response.status) + L".";
    return result;
  }
  if (const auto tag = update::TagFromLocation(response.location)) {
    result.kind = Latest::Kind::kFound;
    result.tag = *tag;
  } else {
    result.kind = Latest::Kind::kNoRelease;
  }
  return result;
}

Latest FetchLatestWithin(DWORD milliseconds) {
  // A promise and not std::async: the future std::async hands back blocks in
  // its destructor until the work is done, which is the wait this avoids.
  auto answer = std::make_shared<std::promise<Latest>>();
  std::future<Latest> future = answer->get_future();
  std::thread([answer, milliseconds] {
    answer->set_value(FetchLatest(milliseconds));
  }).detach();
  if (future.wait_for(std::chrono::milliseconds(milliseconds)) !=
      std::future_status::ready) {
    Latest late;
    late.error = L"server neodpovedal včas.";
    return late;
  }
  return future.get();
}

Latest FetchLatestAsked(HWND owner) {
  // Shared with the worker, which may outlive the dialog if the user cancels.
  struct Asked {
    std::atomic<bool> finished{false};
    Latest latest;  // written before `finished`
  };
  auto job = std::make_shared<Asked>();
  std::thread([job] {
    job->latest = FetchLatest(0);
    job->finished = true;
  }).detach();

  TASKDIALOGCONFIG config = {};
  config.cbSize = sizeof(config);
  config.hwndParent = owner;
  config.dwFlags = TDF_SHOW_MARQUEE_PROGRESS_BAR | TDF_CALLBACK_TIMER |
                   TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
  config.dwCommonButtons = TDCBF_CANCEL_BUTTON;
  config.pszWindowTitle = L"Eureka A4";
  config.pszMainInstruction = L"Zisťujem, či je k dispozícii novšia verzia…";
  config.lpCallbackData = reinterpret_cast<LONG_PTR>(job.get());
  config.pfCallback = [](HWND dialog, UINT notification, WPARAM, LPARAM,
                         LONG_PTR data) -> HRESULT {
    auto* asked = reinterpret_cast<Asked*>(data);
    if (notification == TDN_CREATED)
      SendMessageW(dialog, TDM_SET_PROGRESS_BAR_MARQUEE, TRUE, 0);
    if (notification == TDN_TIMER && asked->finished)
      SendMessageW(dialog, TDM_CLICK_BUTTON, IDCANCEL, 0);
    return S_OK;
  };
  TaskDialogIndirect(&config, nullptr, nullptr, nullptr);
  // The button that closes it is the same either way; what tells the two
  // apart is whether the answer was in.
  if (!job->finished) {
    Latest cancelled;
    cancelled.kind = Latest::Kind::kCancelled;
    return cancelled;
  }
  return job->latest;
}

Choice AskToUpdate(HWND owner, const std::wstring& tag, bool restartsItself) {
  constexpr int kUpdate = 100;
  constexpr int kLater = 101;
  constexpr int kSkip = 102;
  const TASKDIALOG_BUTTON buttons[] = {
      {kUpdate, L"&Aktualizovať"},
      {kLater, L"&Neskôr"},
      {kSkip, L"&Preskočiť túto verziu"},
  };
  const std::wstring instruction = L"Je k dispozícii verzia " + Bare(tag) + L".";
  const std::wstring content =
      L"Máte verziu " + std::wstring(version::Current()) + L"." +
      (restartsItself ? L" Po aktualizácii sa emulátor spustí znova." : L"");
  std::wstring page = update::kReleasePage;
  page.resize(page.size() - std::wcslen(L"latest"));
  const std::wstring footer = L"<a href=\"" + page + L"tag/" + tag +
                              L"\">Čo je nové vo verzii " + Bare(tag) + L"</a>";

  TASKDIALOGCONFIG config = {};
  config.cbSize = sizeof(config);
  config.hwndParent = owner;
  config.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION |
                   TDF_POSITION_RELATIVE_TO_WINDOW;
  config.pszWindowTitle = L"Eureka A4";
  config.pszMainInstruction = instruction.c_str();
  config.pszContent = content.c_str();
  config.pszFooter = footer.c_str();
  config.cButtons = ARRAYSIZE(buttons);
  config.pButtons = buttons;
  config.nDefaultButton = kUpdate;
  config.pfCallback = [](HWND, UINT notification, WPARAM, LPARAM lParam,
                         LONG_PTR) -> HRESULT {
    if (notification == TDN_HYPERLINK_CLICKED)
      ShellExecuteW(nullptr, L"open", reinterpret_cast<LPCWSTR>(lParam), nullptr,
                    nullptr, SW_SHOWNORMAL);
    return S_OK;
  };
  int pressed = kLater;
  if (FAILED(TaskDialogIndirect(&config, &pressed, nullptr, nullptr)))
    return Choice::kLater;
  if (pressed == kUpdate) return Choice::kUpdate;
  if (pressed == kSkip) return Choice::kSkip;
  return Choice::kLater;
}

Installed DownloadAndInstall(HWND owner, const std::wstring& tag, bool restart) {
  auto job = std::make_shared<Download>();
  job->tag = tag;
  job->target = ThisExe();
  job->restart = restart;
  std::thread([job] { RunDownload(job); }).detach();

  const std::wstring instruction = L"Sťahujem verziu " + Bare(tag) + L"…";
  TASKDIALOGCONFIG config = {};
  config.cbSize = sizeof(config);
  config.hwndParent = owner;
  config.dwFlags = TDF_SHOW_PROGRESS_BAR | TDF_CALLBACK_TIMER |
                   TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
  config.dwCommonButtons = TDCBF_CANCEL_BUTTON;
  config.pszWindowTitle = L"Eureka A4";
  config.pszMainInstruction = instruction.c_str();
  config.pfCallback = ProgressCallback;
  config.lpCallbackData = reinterpret_cast<LONG_PTR>(job.get());
  TaskDialogIndirect(&config, nullptr, nullptr, nullptr);

  // Still pending means the dialog closed before the download did: cancelled.
  switch (job->outcome) {
    case Download::Outcome::kPending:
      return Installed::kCancelled;
    case Download::Outcome::kRestarted:
      return Installed::kRestarted;
    case Download::Outcome::kInstalled:
      if (restart)
        MessageBoxW(owner, (L"Nová verzia je nainštalovaná, ale " +
                            job->restartError).c_str(),
                    L"Eureka A4", MB_OK | MB_ICONWARNING);
      return Installed::kInstalled;
    case Download::Outcome::kNotWritable: {
      const std::wstring question =
          L"Do priečinka " + job->target.parent_path().wstring() +
          L" sa nedá zapisovať, preto sa emulátor nemôže aktualizovať sám.\r\n\r\n"
          L"Otvoriť stránku s novou verziou?";
      if (MessageBoxW(owner, question.c_str(), L"Eureka A4",
                      MB_YESNO | MB_ICONWARNING) == IDYES)
        OpenReleasePage();
      return Installed::kFailed;
    }
    case Download::Outcome::kFailed:
      break;
  }
  MessageBoxW(owner, (L"Aktualizácia sa nepodarila: " + job->error).c_str(),
              L"Eureka A4", MB_OK | MB_ICONWARNING);
  return Installed::kFailed;
}

bool Restart(std::wstring& error) { return StartNew(ThisExe(), error); }

void RemoveLeftover() {
  fs::path old = ThisExe();
  if (old.empty()) return;
  old += L".old";
  DeleteFileW(old.c_str());
}

std::wstring Today() {
  SYSTEMTIME now = {};
  GetLocalTime(&now);
  wchar_t text[16] = {};
  std::swprintf(text, 16, L"%04u-%02u-%02u", now.wYear, now.wMonth, now.wDay);
  return text;
}

}  // namespace updater
