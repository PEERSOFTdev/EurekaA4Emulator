# Nástroje na analýzu ROM

Čistý Python 3, bez závislostí. ROM sa hľadá cez premennú `A4ROM`,
inak na `C:\b\a4rom.dmp`.

| nástroj | čo robí |
|---|---|
| `z180dis.py` | disassembler Z80/Z180 (knižnica, importuje sa) |
| `z180len.py` | dĺžky inštrukcií (knižnica) |
| `dis.py LO:HI [LO:HI …]` | disassembluje rozsahy fyzických adries |
| `strings_kam.py MIN LO HI` | reťazce s dekódovaním Kamenických/KEYBCS2 |
| `rommap.py` | entropia a zloženie ROM po 4 KB blokoch |
| `ports.py` | mapa I/O portov lineárnym rozmetaním inštrukcií |
| `latches.py` | odkazy na tiene riadiacich latchov + súhrn po bitoch |
| `melodies.py` | vyrenderuje melódie z ROM do `../audio/melodie/` |
| `speech_wav.py [ROM] [LO] [HI] [RATE]` | dáta reči (`20000h–40000h`) do `../audio/` ako 8-bit WAV pri φ/(20·41) ≈ 7493 Hz |
| `drive_steps.py IN OUT T0 T1 KROK…` | vystrihne kroky hlavičky z nahrávky mechaniky a odčíta z nich šum točenia (HANDOFF 6.54); ROM nepotrebuje |

## `dumprom/` — program, ktorý ROM vyčíta zo skutočnej Eureky

`dumprom.s` je zdroják `DUMPROM.COM` v assembleri Z180, `DUMPROM.COM` je
hotový program, ktorý ide do vydania; ako ho majiteľ stroja použije, hovorí
README. Je v ňom len náš kód — volania BDOS a registre HD64180 sú rozhranie
(`PRAVNE.md`, bod 5).

`build.sh` ho zostaví GNU binutils pre Z80 (`apt install binutils-z80`)
a skontroluje, že program končí pod svojím bufferom na `2000h`. Treba ho len
po zmene zdrojáka.

Overené v emulátore (HANDOFF 6.55): na našej ROM zapíše `A4ROM.DMP` s MD5
`9aa101ab69fc367e114e1a84b08feea1`, aj s osadenou rozšírenou RAM. Vetvy pre
27C256, Advanced English, neznáme rozloženie a nečakanú mapu pamäte
emulátor nemodeluje; overené sú mutáciou.

Adresy sú vždy **fyzické** (offset v dumpe), nie logické. Prepočet na
logické závisí od stavu MMU: pri common area 1 s CBR=0Bh platí
fyzická = logická + B000h.

```
set A4ROM=C:\b\a4rom.dmp
python dis.py 19837:19880
python strings_kam.py 8 0xd000 0xe000
python latches.py
```
