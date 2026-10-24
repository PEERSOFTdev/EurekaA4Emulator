# Pôvod zvukov disketovej mechaniky

Zvuky sú nahrávky **skutočnej disketovej mechaniky Eureky A4**. Nahral ich
**Luboš Krejča**. Všetky súbory sú WAV, 48 kHz, mono, 16 bit.

## Licencia

Nahrávky sa šíria pod rovnakou licenciou ako celý projekt, **MIT**
(`LICENSE.txt` v koreni). Autorom nahrávok je Luboš Krejča.

Autor s tým súhlasil v októbri 2026, ústne, v rozhovore s majiteľom
projektu: súhlasí, aby sa nahrávky stali súčasťou projektu a šírili sa
pod licenciou MIT, a želá si, aby sa pri nich uvádzalo jeho meno.

## Ako vznikli

Z nahrávok autora ich vystrihol skript `tools/sada-mechaniky.sh`
(`ffmpeg`), ktorý z tých istých nahrávok zostaví bajtovo rovnakú sadu.
Celá sada je zosilnená jedným spoločným ziskom 1,4, takže zvuky medzi
sebou zostali v pomere, v akom zneli na stroji:

- `motor-rozbeh.wav` — prvých 0,3 s nahrávky „Diskove funkce“, so zapnutím
  motora;
- `motor-slucka.wav` — 0,3 až 1,05 s tej istej nahrávky, ustálený motor
  pred krokom; jej koniec je prelínaný so začiatkom, aby sa dala hrať
  dookola;
- `motor-dobeh.wav` — 2,0 s až koniec tej istej nahrávky, kde motor slabne;
  nahrávka končí skôr než motor, preto sa na konci stlmí;
- `krok-1.wav` — nahrávka „Pohyb7“, jeden krok hlavičky;
- `presun.wav` — nahrávka „pohyb1“, hrabanie hlavičky pri rýchlosti kroku
  6 ms, ktorú používa ROM.

Podrobnosti aj meranie, ktoré k výberu viedlo, sú v HANDOFF 6.54.

## Predchádzajúca sada

Do 24. 10. 2026 tu bola sada z cudzích 3,5" mechaník (motor zo vzoriek
Flopster od autora Shiru pod licenciou CC BY, kroky z Wikimedia Commons
a hrabanie z BigSoundBank pod CC0). Je v histórii gitu, naposledy
v commite `2af55a9`; jej pôvod a licencie sú v tamojšom `PUVOD.md`.
