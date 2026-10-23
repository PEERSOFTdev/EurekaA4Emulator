# Právne postavenie projektu

> **Summary in English.** The Eureka A4 Emulator is an independent work
> that reproduces the functionality of the Robotron Eureka A4 hardware.
> It contains no firmware of the machine, in any form, and none of its
> releases ever will. The firmware is a copyright work, presumed to belong
> to Robotron Pty. Ltd. (Australia). The emulator runs only firmware that
> its user supplies. The project provides a tool with which the owner of a
> Eureka A4 can make a copy of the firmware of their own machine, relying
> on the exceptions for lawful users of computer programs in Czech, Slovak
> and Australian law and in Directive 2009/24/EC. The technical manual and
> the developer diskette are not part of the project; facts about the
> machine's interfaces are described in the project's own words. The
> binding text of this statement is the Slovak one below.

Toto vyhlásenie opisuje, čo projekt Eureka A4 Emulator obsahuje a čo nie,
na akom právnom základe stojí, čo podľa uvedených predpisov smie
oprávnený používateľ firmvéru a k čomu sa zaväzuje projekt sám.

## 1. Predmet a rozhodné právo

1.1. Vyhlásenie sa vzťahuje na repozitár projektu, na všetky jeho vydania
a na nástroje, ktoré projekt poskytuje.

1.2. Vyhlásenie vychádza z týchto predpisov:

- zákon č. 121/2000 Sb., o právu autorském, o právech souvisejících
  s právem autorským a o změně některých zákonů (autorský zákon), Česká
  republika (ďalej „český autorský zákon“);
- zákon č. 185/2015 Z. z. Autorský zákon, Slovenská republika (ďalej
  „slovenský autorský zákon“);
- Copyright Act 1968, Austrálsky zväz (ďalej „austrálsky Copyright Act“);
- smernica Európskeho parlamentu a Rady 2009/24/ES o právnej ochrane
  počítačových programov (ďalej „smernica“), ktorú oba uvedené autorské
  zákony preberajú.

1.3. O porušení autorského práva sa rozhoduje podľa práva štátu, v ktorom
sa ochrana požaduje. Sprístupnenie na internete je úkonom vo všetkých
štátoch naraz. Vyhlásenie preto opisuje postavenie, ktoré obstojí podľa
každého z uvedených právnych poriadkov súčasne.

## 2. Emulátor

2.1. Emulátor je samostatné dielo autorov projektu. Šíri sa pod licenciami
uvedenými v `LICENSE.txt`; časti od tretích strán so slobodnou licenciou
sú označené tam, kde sa nachádzajú.

2.2. Emulátor napodobňuje funkciu technického vybavenia počítača
Eureka A4: procesor, pamäť a jej stránkovanie, vstupno-výstupné porty,
časovače, prerušenia, klávesnice, zvukový výstup, disketovú mechaniku
a sériové rozhranie. Funkcia počítačového programu či zariadenia,
myšlienky a princípy, na ktorých je založený ktorýkoľvek prvok programu
vrátane jeho rozhraní, programovací jazyk a formát dátových súborov nie sú
predmetom autorskoprávnej ochrany:

- český autorský zákon, § 65 ods. 2;
- slovenský autorský zákon, § 5 písm. a) a § 87 ods. 1;
- smernica, čl. 1 ods. 2;
- rozsudok Súdneho dvora EÚ C-406/10 *SAS Institute*.

2.3. Zdrojový kód emulátora neobsahuje firmvér počítača Eureka A4 ani jeho
časť, preklad, úpravu alebo prepis do iného jazyka. Kde emulátor
odpovedá na volania firmvéru namiesto neho, robí to podľa rozhrania
týchto volaní, nie podľa ich implementácie vo firmvéri.

## 3. Firmvér (ROM)

3.1. Firmvér uložený v pamäti ROM počítača Eureka A4 je počítačový program
chránený autorským právom. Majetkové práva k nemu pravdepodobne vykonáva
spoločnosť Robotron Pty. Ltd. (Austrália) ako zamestnávateľ jeho
autorov:

- český autorský zákon, § 58 ods. 1 a 7;
- slovenský autorský zákon, § 90 ods. 1 a 4;
- austrálsky Copyright Act, s 35(6).

Spoločnosť Robotron Pty. Ltd. je podľa austrálskeho registra (Australian
Business Register) naďalej zapísaná ako činná. Projekt nemá vedomosť o tom,
či majetkové práva k firmvéru previedla na inú osobu.

3.2. Majetkové práva trvajú počas života autora a 70 rokov po jeho smrti
(český autorský zákon § 27 ods. 1, slovenský autorský zákon § 32 ods. 1,
austrálsky Copyright Act s 33(2)). Vek firmvéru, skutočnosť, že sa stroj už
nevyrába, ani to, že ho nikto neponúka na predaj, na trvaní ochrany nič
nemenia.

3.3. Repozitár ani žiadne vydanie projektu firmvér neobsahuje, a to
v nijakej podobe: ani ako samostatný súbor, ani zakódovaný, skomprimovaný,
rozdelený na časti, vyjadrený ako rozdiel voči inému súboru, vložený do
spustiteľného súboru, ani tak, že by ho emulátor alebo nástroj projektu
sám stiahol z iného zdroja. Každá z týchto podôb je rozmnoženinou toho
istého programu.

3.4. Žiadne ustanovenie uvedených predpisov nedovoľuje firmvér šíriť bez
súhlasu nositeľa práv. Na tom nič nemení nekomerčná povaha projektu ani
to, že príjemca vlastní skutočný počítač Eureka A4. Vlastníctvo stroja dáva
oprávnenia jeho vlastníkovi, nie tomu, kto by mu rozmnoženinu poskytol.

3.5. To isté platí pre všetko, čo z firmvéru vznikne nástrojmi projektu
alebo inak, najmä pre úplné alebo podstatné výpisy jeho disassembly a pre
zvuky, texty a tabuľky z neho vyňaté v úplnom alebo podstatnom rozsahu.
Jednotlivé krátke hlásenia, ktorými stroj odpovedá, projekt uvádza tam,
kde nimi označuje správanie, ktoré emulátor napodobňuje alebo ktoré testy
overujú. Kontrolný súčet a dĺžka
súboru s firmvérom rozmnoženinou programu nie sú a projekt ich uvádza, aby
si každý mohol overiť svoj vlastný súbor.

## 4. Rozmnoženina firmvéru z vlastného stroja

4.1. Emulátor pracuje len s firmvérom, ktorý mu dodá jeho používateľ.
Projekt predpokladá, že ide o rozmnoženinu, ktorú si používateľ vyhotovil
z počítača Eureka A4, ktorý vlastní.

4.2. Firmvér je v počítači Eureka A4 zachytený ako hmotná rozmnoženina
programu. Prvým predajom stroja nositeľom práv alebo s jeho súhlasom sa
právo rozširovať túto rozmnoženinu vyčerpalo, takže stroj aj s firmvérom
sa smie ďalej predávať (smernica čl. 4 ods. 2, český autorský zákon § 14
ods. 2, slovenský autorský zákon § 88). Kto ho takto nadobudol, a to aj
z druhej ruky, je oprávneným používateľom firmvéru, ktorý je v ňom
uložený (český autorský zákon § 66 ods. 6 prvá veta, slovenský autorský
zákon § 89 ods. 1, smernica čl. 5 ods. 1; v tom istom zmysle rozsudok
Súdneho dvora EÚ C-128/11 *UsedSoft*). Vlastníctvo veci, v ktorej je
dielo zachytené, však samo autorské právo neprenáša (slovenský autorský
zákon § 33 ods. 1).

4.3. Oprávnený používateľ smie bez súhlasu nositeľa práv:

- vyhotoviť záložnú rozmnoženinu, ak je potrebná na využitie programu
  (český autorský zákon § 66 ods. 1 písm. c), slovenský autorský zákon
  § 89 ods. 2 písm. b), smernica čl. 5 ods. 2);
- podľa austrálskeho Copyright Act vyhotoviť z pôvodného exemplára
  rozmnoženinu výlučne pre vlastné použitie, aby ju používal namiesto
  pôvodného exemplára, ktorý si ponechá (s 47C(1)(c)(i)).

Tieto oprávnenia nemožno zmluvou vylúčiť ani obmedziť (český autorský
zákon § 66 ods. 6 druhá veta, smernica čl. 8 druhý odsek, austrálsky
Copyright Act s 47H).

Pri spustení rozmnoženiny v emulátore sa projekt opiera aj o oprávnenie
vyhotoviť rozmnoženinu, ktorá je nevyhnutná na využitie programu
v súlade s jeho určením (český autorský zákon § 66 ods. 1 písm. b),
slovenský autorský zákon § 89 ods. 2 písm. a), smernica čl. 5 ods. 1).
Toto oprávnenie platí, ak nie je dohodnuté inak, a nie je ustálené, či
sa vzťahuje na beh mimo stroja, pre ktorý bol firmvér určený.

4.4. Projekt sa opiera len o tieto ustanovenia, nie o výnimku pre osobnú
alebo súkromnú potrebu. Český autorský zákon výslovne považuje za užitie
aj užitie počítačového programu pre osobnú potrebu fyzickej osoby (§ 30
ods. 3).

4.5. Ustanovenia bodu 4.3 dovoľujú rozmnoženinu len na využitie programu
tým, kto ju vyhotovil. Jej odovzdanie inej osobe nedovoľuje žiadne z nich,
a to ani inému vlastníkovi počítača Eureka A4, ani na zálohovanie alebo
testovanie u iného (bod 3.4). Kto stroj predá alebo inak prevedie,
prestáva byť oprávneným používateľom (bod 4.2), a ustanovenia bodu 4.3
mu ďalšie užívanie rozmnoženiny nedovoľujú.

4.6. Rozmnoženina získaná od inej osoby nie je rozmnoženinou podľa bodu 4.3,
ani keď jej príjemca vlastní vlastný stroj (austrálsky Copyright Act
s 47C(4)(a)).

## 5. Nástroj na vyhotovenie rozmnoženiny

5.1. Nástroj, ktorým sa z počítača Eureka A4 vyhotovuje rozmnoženina
firmvéru, je samostatné dielo autorov projektu a šíri sa pod licenciou
projektu. Neobsahuje firmvér ani jeho časť.

5.2. Firmvér počítača Eureka A4 nie je chránený technickým prostriedkom
ochrany. Nástroj žiadny taký prostriedok neobchádza, nie je teda
prostriedkom podľa čl. 7 ods. 1 písm. c) smernice. Číta pamäť stroja
prostriedkami, ktoré stroj poskytuje každému programu, ktorý na ňom beží.

5.3. Nástroj je určený oprávnenému používateľovi na úkon podľa bodu 4.3,
na jeho vlastnom stroji.

## 6. Technický manuál a vývojárska disketa

6.1. Technický manuál k počítaču Eureka A4 a vývojárska disketa k nemu sú
diela chránené autorským právom. Disketa obsahuje počítačové programy
nositeľa práv k firmvéru a program Turbo Pascal spoločnosti Borland,
ktorý patrí tretej strane.

6.2. Repozitár ani vydania neobsahujú manuál, jeho naskenované strany,
podstatné časti jeho textu, tabuliek alebo schém, ani žiadny súbor
z vývojárskej diskety. Nástroje projektu, ktoré manuál potrebujú, ho
hľadajú mimo repozitára, u toho, kto ich spúšťa.

6.3. Autorské právo chráni vyjadrenie manuálu, nie technické skutočnosti,
ktoré oznamuje. Adresy, porty, významy bitov, mená signálov, čísla volaní,
zoznamy parametrov a formáty súborov sú opisom stroja a jeho rozhraní
(bod 2.2). Projekt ich používa v kóde a opisuje ich vlastnými slovami
s odkazom na prameň.

6.4. Manuál je zverejnené dielo. Podľa vlastného textu sa dodával spolu
s dvoma disketami, na ktorých je aj program a návod tretej strany
poskytnuté s jej súhlasom, a obracia sa na vlastníka počítača Eureka A4 ako na
zákazníka, ktorého odkazuje na zástupcu nositeľa práv. Krátke citácie
z neho sú preto prípustné v rozsahu
odôvodnenom účelom citácie a s uvedením autora, názvu a prameňa:

- český autorský zákon, § 31 ods. 1;
- slovenský autorský zákon, § 37;
- austrálsky Copyright Act, s 40 a s 41 (primerané použitie na účel
  výskumu alebo štúdia, kritiky alebo recenzie, s dostatočným uvedením
  diela).

6.5. Citovať nemožno programy z vývojárskej diskety. Na počítačové
programy sa citácia podľa českého autorského zákona nevzťahuje (§ 66
ods. 7) a ani ostatné uvedené predpisy na ich šírenie citáciou nedávajú
základ.

## 7. Skúmanie firmvéru

7.1. Oprávnený používateľ smie pozorovať, skúmať a skúšať fungovanie
programu, aby zistil myšlienky a princípy, na ktorých je založený, a to pri
jeho zavedení, zobrazení, spustení, prenose alebo uložení:

- český autorský zákon, § 66 ods. 1 písm. d);
- slovenský autorský zákon, § 89 ods. 2 písm. c);
- smernica, čl. 5 ods. 3;
- austrálsky Copyright Act, s 47B(3).

Väčšina poznatkov projektu vznikla takto, pozorovaním bežiaceho firmvéru
v emulátore. Zverejnenie takých poznatkov nie je obmedzené, lebo opisujú
myšlienky a princípy, nie vyjadrenie programu.

7.2. Rozmnoženie kódu alebo preklad jeho formy (disassembly) je prípustné,
ak je nevyhnutné na získanie informácií potrebných na dosiahnutie
interoperability samostatne vytvoreného programu, tieto informácie neboli
predtým ľahko dostupné a týka sa len potrebných častí programu:

- český autorský zákon, § 66 ods. 1 písm. e);
- slovenský autorský zákon, § 89 ods. 3;
- smernica, čl. 6;
- austrálsky Copyright Act, s 47D.

Samostatne vytvoreným programom je emulátor a firmvér je program, s ktorým
spolupracuje.

7.3. Informácie získané podľa bodu 7.2 projekt používa len na
interoperabilitu emulátora s firmvérom a zverejňuje ich len v rozsahu,
ktorý na ňu treba. Nepoužíva ich na vytvorenie programu podobného
firmvéru vo vyjadrení.

7.4. Projekt nezverejňuje výpisy disassembly firmvéru. Dokumentácia
obsahuje nanajvýš krátke úryvky niekoľkých inštrukcií tam, kde bez nich
nemožno opísať, ako firmvér ovláda technické vybavenie stroja.

## 8. Príspevky

8.1. Projekt neprijíma príspevky, ktoré obsahujú:

- firmvér alebo jeho časť v akejkoľvek podobe podľa bodu 3.3;
- výpisy disassembly firmvéru nad rozsah bodu 7.4;
- bajty firmvéru ako testovacie dáta;
- kód prepísaný z firmvéru alebo z jeho disassembly do iného jazyka;
- text, tabuľky, schémy alebo obrázky z technického manuálu nad rozsah
  citácie podľa bodu 6.4;
- súbory z vývojárskej diskety.

8.2. Testy pracujú s programami, ktoré napísali autori projektu, alebo
s firmvérom, ktorý dodá ten, kto testy spúšťa, a ktorý repozitár
neobsahuje.

8.3. Autor príspevku zodpovedá za to, že k nemu má práva potrebné na jeho
šírenie pod licenciou projektu.

8.4. Poznatky, ktoré sa opierajú o firmvér, projekt prijíma len od
oprávneného používateľa firmvéru podľa bodu 4.2, získané z jeho vlastnej
rozmnoženiny. Od toho, kto počítač Eureka A4 nevlastní, prijíma to, čo
zistil pozorovaním stroja, ktorý mu na to prenechal jeho vlastník, bez
toho, aby z neho vyhotovil rozmnoženinu firmvéru, alebo výsledky, ktoré
overil vlastník stroja na svojej rozmnoženine.

## 9. Záverečné ustanovenie

Toto vyhlásenie opisuje právne postavenie projektu a pravidlá, ktorými sa
riadi. Nikomu nepriznáva oprávnenia nad rámec, ktorý mu dáva zákon, a
nenahrádza posúdenie konkrétneho prípadu.
