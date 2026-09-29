#include "update.h"

namespace update {

namespace {

bool IsHex(char ch) {
  return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
         (ch >= 'A' && ch <= 'F');
}

}  // namespace

std::optional<std::wstring> TagFromLocation(std::wstring_view location) {
  constexpr std::wstring_view kMarker = L"/releases/tag/";
  const std::size_t at = location.find(kMarker);
  if (at == std::wstring_view::npos) return std::nullopt;
  std::wstring_view tag = location.substr(at + kMarker.size());
  tag = tag.substr(0, tag.find_first_of(L"?#/"));
  // Only a tag that is a release number.  Whatever else might sit there is not
  // something this program published, so it is not offered.
  if (!version::Parse(tag)) return std::nullopt;
  return std::wstring(tag);
}

std::optional<std::string> HashFor(std::string_view sums, std::string_view file) {
  while (!sums.empty()) {
    const std::size_t end = sums.find('\n');
    std::string_view line = sums.substr(0, end);
    sums = end == std::string_view::npos ? std::string_view{} : sums.substr(end + 1);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (line.size() < 66) continue;
    const std::string_view hash = line.substr(0, 64);
    std::string_view name = line.substr(64);
    // Two spaces in text mode, space and '*' in binary mode.
    if (name.starts_with("  ") || name.starts_with(" *")) name.remove_prefix(2);
    else continue;
    if (name != file) continue;
    std::string result;
    for (char ch : hash) {
      if (!IsHex(ch)) return std::nullopt;
      result += ch >= 'A' && ch <= 'F' ? static_cast<char>(ch - 'A' + 'a') : ch;
    }
    return result;
  }
  return std::nullopt;
}

bool ShouldOffer(std::optional<version::Number> current,
                 std::wstring_view latestTag, std::wstring_view skipped) {
  const std::optional<version::Number> latest = version::Parse(latestTag);
  if (!current || !latest || *latest <= *current) return false;
  const std::optional<version::Number> declined = version::Parse(skipped);
  return !declined || *latest != *declined;
}

bool CheckDue(std::wstring_view lastCheck, std::wstring_view today) {
  return lastCheck != today;
}

}  // namespace update
