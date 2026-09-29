// Versions and updates: what counts as a release and which of two is newer.
//
// The failure this guards against is quiet in both directions.  A comparison
// done as text puts "2026.9.5" above "2026.10.1", so from October on nobody is
// offered the update and nothing says so.  And a development build that parses
// as a release would compare itself with the published one and offer to
// replace itself with an older EXE.
//
// No ROM, no network, no files.

#include <iostream>
#include <string>

#include "version.h"

namespace {

int checks = 0;
int failures = 0;

void Check(bool passed, const std::string& name) {
  ++checks;
  if (!passed) ++failures;
  std::cout << (passed ? "  ok   " : "  CHYBA ") << name << "\n";
}

bool Is(std::wstring_view text, int year, int month, int serial) {
  const auto number = version::Parse(text);
  return number && *number == version::Number{year, month, serial};
}

void ReleasesParse() {
  Check(Is(L"2026.9.1", 2026, 9, 1), "2026.9.1 je vydanie");
  Check(Is(L"v2026.9.1", 2026, 9, 1), "znacka v2026.9.1 je to iste vydanie");
  Check(Is(L"2026.12.31", 2026, 12, 31), "dvojciferny mesiac aj poradie");
}

void EverythingElseIsNotARelease() {
  const wchar_t* rejected[] = {
      L"",
      L"v",
      L"2026.9",
      L"2026.9.",
      L"2026..1",
      L".9.1",
      L"2026.9.1.0",
      L"2026.9.x",
      L"2026.9.1-5-gabc1234",
      L"2026.9.1-dirty",
      L"0.0.0-69e070f",
      L" 2026.9.1",
      L"2026.9.1 ",
      L"vv2026.9.1",
      L"+2026.9.1",
      L"2026.9.123456",
  };
  for (const wchar_t* text : rejected) {
    std::string name = "nie je vydanie: \"";
    for (const wchar_t* ch = text; *ch; ++ch) name += static_cast<char>(*ch);
    Check(!version::Parse(text), name + "\"");
  }
}

void ComparedAsNumbers() {
  const auto september = version::Parse(L"2026.9.5");
  const auto october = version::Parse(L"2026.10.1");
  Check(september && october && *october > *september,
        "2026.10.1 je novsia nez 2026.9.5 (cisla, nie text)");
  const auto second = version::Parse(L"2026.9.10");
  const auto first = version::Parse(L"2026.9.9");
  Check(second && first && *second > *first, "2026.9.10 je novsia nez 2026.9.9");
  const auto nextYear = version::Parse(L"2027.1.1");
  const auto december = version::Parse(L"2026.12.9");
  Check(nextYear && december && *nextYear > *december,
        "2027.1.1 je novsia nez 2026.12.9");
  Check(version::Parse(L"v2026.9.1") == version::Parse(L"2026.9.1"),
        "s v aj bez v je to ta ista verzia");
}

// Only the development suffix may keep the build from having a number.  The
// value itself depends on the checkout, so this checks the rule, not the text.
void CurrentIsConsistent() {
  const std::wstring_view current = version::Current();
  Check(!current.empty(), "aktualna verzia nie je prazdna");
  const bool looksLikeRelease = current.find(L'-') == std::wstring_view::npos;
  Check(version::CurrentNumber().has_value() == looksLikeRelease,
        "cislo ma prave zostavenie bez pripony");
}

}  // namespace

int main() {
  ReleasesParse();
  EverythingElseIsNotARelease();
  ComparedAsNumbers();
  CurrentIsConsistent();

  std::cout << (failures == 0 ? "PASS" : "FAIL")
            << " mode=UPDATE kontrol=" << checks << " chyb=" << failures
            << "\n";
  return failures == 0 ? 0 : 1;
}
