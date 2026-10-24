# Ako urobiť vlastné zvuky disketovej mechaniky

Tento priečinok je sada zvukov, ktorú emulátor Eureky A4 hrá, keď je
v Nastaveniach zapnutá **Verná disketová mechanika (zvuk aj rýchlosť)**.
Sú to nahrávky skutočnej mechaniky Eureky. Rovnaká sada je zabudovaná priamo
v emulátore. Táto kópia slúži ako vzor: vypočujte si ju, nahraďte súbory
vlastnými a emulátor bude hrať vaše.

Pôvod a licencia súborov sú v `PUVOD.md`. Ak sadu ďalej šírite, posielajte
ho s ňou.

## Kam sadu dať

Priečinok sa musí volať **`zvuky-mechaniky`** a ležať vedľa súboru
s nastaveniami emulátora `nastavenia.txt`:

- v priečinku `config` vedľa `EurekaA4Emulator.exe`, ak taký priečinok
  existuje (prenosná inštalácia),
- inak v `%APPDATA%\EurekaA4`.

Emulátor ho načíta **pri štarte**, takže po výmene súborov ho treba zavrieť
a znovu spustiť. Keď sa vlastná sada nedá načítať a verná mechanika je
zapnutá, emulátor povie, ktorý súbor a prečo, a hrá zabudovanú sadu.

## Formát

Každý súbor je **WAV, PCM, 48 000 Hz, mono, 16 bitov**. Iný formát emulátor
odmietne a neprevzorkuje ho — zvuk s inou frekvenciou by hral v nesprávnej
výške. Sada musí byť **celá**: chýbajúci súbor znamená zabudovanú sadu.

## Súbory a čo robia

Emulátor zvuky nestrihá ani nenaťahuje, len ich spúšťa vo chvíľach, keď
mechanika naozaj niečo robí. Časy nižšie sú časy skutočnej mechaniky Eureky
(radič WD1772, 300 otáčok za minútu, krok 6 ms).

- **`motor-rozbeh.wav`** — motor sa zapína. Hrá sa raz, keď sa stojaci
  motor zapne, a hneď za ním pokračuje slučka, takže koniec rozbehu má
  nadväzovať na začiatok slučky. Mechanika čaká na roztočenie **1,2 s**, až
  potom pohne hlavičkou; motor Eureky je však naplno takmer hneď, a vzor má
  preto len 0,3 s.
- **`motor-slucka.wav`** — motor beží naplno. Hrá sa **dookola**, takže
  koniec musí plynulo nadväzovať na začiatok, bez cvaknutia a bez zmeny
  hlasitosti. Dĺžka nie je predpísaná; vzor má 0,75 s. Motor beží ešte
  **1,8 s** po poslednej práci.
- **`motor-dobeh.wav`** — motor dobieha do ticha. Hrá sa, keď motor
  zastane. Keď mechanika počas dobehu znovu začne pracovať, emulátor skočí
  rovno do slučky (s 10 ms prelínaním), lebo disketa sa ešte točí; vzor má
  2,85 s.
- **`krok-1.wav`, `krok-2.wav` …** — **jeden krok hlavičky** o jednu stopu.
  Krokov môže byť ľubovoľne veľa, najmenej `krok-1.wav`; emulátor ich berie
  po poradí, kým nejaké číslo nechýba, a pri hraní ich strieda. Viac
  záberov toho istého kroku zabráni tomu, aby rad krokov znel ako jedna
  pečiatka — ale **len rovnaké kroky**: iný zvuk medzi nimi, napríklad prvý
  pohyb po roztočení, ktorý na Eureke znie ako dvojitý úder, by sa ozýval pri
  každom druhom kroku. Krok má byť **bez zvuku motora** — motor hrá pod ním
  zo slučky. Vzor má 0,1 s, z toho počuť asi 50 ms.
- **`presun.wav`** — **hrabanie** hlavičky pri presune o dve a viac stôp
  naraz. Hrá sa toľko, koľko presun trvá (6 ms na stopu, cez celú disketu
  asi pol sekundy), potom 15 ms doznieva. Dlhší presun než nahrávka ju
  opakuje dookola, takže aj tu pomôže plynulý koniec. Nahrajte ho pri
  rýchlosti kroku 6 ms — klepanie v ňom má prísť každých 6 ms; vzor má 0,56 s.

## Hlasitosť

Emulátor stlmí **celú sadu na štvrtinu** a medzi zvukmi nemení nič.
Nahrávajte preto všetko s rovnakým nastavením nahrávania, v pomere, v akom
to znie na skutočnom stroji, a dosť nahlas — vo vzore má slučka motora
špičku okolo 2 600 (zo 32 767), krok okolo 13 200 a hrabanie okolo 7 100.

Mechanika **mlčí, kým Eureka hovorí**, tak ako na skutočnom stroji: firmvér
počas reči na disketu nesiaha, motor len dobieha.
