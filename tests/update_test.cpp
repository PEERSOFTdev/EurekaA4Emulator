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

#include "update.h"
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

// What github.com/.../releases/latest redirects to.  With no release at all it
// redirects to the release list, and that must not read as a version.
void TagIsReadFromTheRedirect() {
  const auto tag = update::TagFromLocation(
      L"https://github.com/lpintes/EurekaA4Emulator/releases/tag/v2026.9.2");
  Check(tag && *tag == L"v2026.9.2", "znacka z presmerovania na vydanie");
  const auto relative =
      update::TagFromLocation(L"/lpintes/EurekaA4Emulator/releases/tag/v2026.10.1");
  Check(relative && *relative == L"v2026.10.1", "znacka aj z relativnej cesty");
  const auto query = update::TagFromLocation(
      L"https://github.com/x/y/releases/tag/v2026.9.3?from=latest");
  Check(query && *query == L"v2026.9.3", "otaznik za znackou sa odhodi");
  Check(!update::TagFromLocation(L"https://github.com/lpintes/EurekaA4Emulator/releases"),
        "bez vydania (presmerovanie na zoznam) nie je ziadna znacka");
  Check(!update::TagFromLocation(
            L"https://github.com/x/y/releases/tag/v2026.9.3-rc1"),
        "znacka, ktora nie je cislo vydania, sa neponukne");
  Check(!update::TagFromLocation(L""), "prazdna hlavicka nie je znacka");
}

void HashIsReadFromTheSums() {
  const std::string sums =
      "4cad0d27534f799e44214296c72b709f04bc362c7f28d671cafd8b6ff00e5823  "
      "EurekaA4Emulator-2026.9.1.zip\n"
      "CCA07FB737596BD4DC5129A52A07371213CCF9FF18DB6A4BD8E1317203726F7B  "
      "EurekaA4Emulator.exe\r\n"
      "fd9929f90c64315aa5efe8ee7dc200e20f3e0f9e7c1630bd20b29c1baca5f5aa "
      "*eurekaA4Emulator.nvda-addon\n";
  const auto exe = update::HashFor(sums, "EurekaA4Emulator.exe");
  Check(exe && *exe ==
                   "cca07fb737596bd4dc5129a52a07371213ccf9ff18db6a4bd8e1317203726f7b",
        "sucet EXE najdeny, CRLF odhodene, male pismena");
  Check(update::HashFor(sums, "eurekaA4Emulator.nvda-addon").has_value(),
        "binarny tvar s hviezdickou");
  Check(!update::HashFor(sums, "EurekaA4Emulator"), "cast mena nestaci");
  Check(!update::HashFor(sums, "eurekaa4emulator.exe"),
        "meno sa porovnava presne, aj velkost pismen");
  Check(!update::HashFor("xyz  EurekaA4Emulator.exe\n", "EurekaA4Emulator.exe"),
        "kratky riadok nie je sucet");
  Check(!update::HashFor(std::string(64, 'g') + "  EurekaA4Emulator.exe\n",
                         "EurekaA4Emulator.exe"),
        "nehexadecimalny sucet sa odmietne");
}

void OfferRules() {
  const auto current = version::Parse(L"2026.9.1");
  Check(update::ShouldOffer(current, L"v2026.9.2", L""), "novsia sa ponukne");
  Check(update::ShouldOffer(current, L"v2026.10.1", L""),
        "oktober po septembri sa ponukne");
  Check(!update::ShouldOffer(current, L"v2026.9.1", L""), "ta ista sa neponukne");
  Check(!update::ShouldOffer(current, L"v2026.8.4", L""), "starsia sa neponukne");
  Check(!update::ShouldOffer(std::nullopt, L"v2026.9.2", L""),
        "vyvojove zostavenie neponuka nic");
  Check(!update::ShouldOffer(current, L"v2026.9.2", L"2026.9.2"),
        "preskocena sa neponukne");
  Check(update::ShouldOffer(current, L"v2026.9.3", L"2026.9.2"),
        "novsia nez preskocena sa ponukne znova");
  Check(!update::ShouldOffer(current, L"nezmysel", L""),
        "nezmysel od servera sa neponukne");
}

void OnceADay() {
  Check(update::CheckDue(L"", L"2026-09-29"), "prvy raz je kontrola na rade");
  Check(!update::CheckDue(L"2026-09-29", L"2026-09-29"), "v ten isty den nie");
  Check(update::CheckDue(L"2026-09-28", L"2026-09-29"), "na druhy den ano");
}

}  // namespace

int main() {
  ReleasesParse();
  EverythingElseIsNotARelease();
  ComparedAsNumbers();
  CurrentIsConsistent();
  TagIsReadFromTheRedirect();
  HashIsReadFromTheSums();
  OfferRules();
  OnceADay();

  std::cout << (failures == 0 ? "PASS" : "FAIL")
            << " mode=UPDATE kontrol=" << checks << " chyb=" << failures
            << "\n";
  return failures == 0 ? 0 : 1;
}
