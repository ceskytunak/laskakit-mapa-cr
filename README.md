# LaskaKit Mapa ČR — firmware s webovou appkou a hrou „Zachraň město"

Firmware pro **[LaskaKit Interaktivní mapu ČR (WS2812B)](https://www.laskakit.cz/laskakit-interaktivni-mapa-cr-ws2812b/)** — desku ve tvaru České republiky s 72 adresovatelnými RGB LED (ESP32, datový pin GPIO 25).

Tento repozitář je **fork firmwaru [KRtekTM/Laskakit_MapaCR](https://github.com/KRtekTM/Laskakit_MapaCR)** rozšířený o **herní režim „Zachraň město"** přímo ve webovém ovládání mapy. Mezi zobrazením dat na mapě a hrou se přepíná jedním tlačítkem v prohlížeči.

> Credits pro původní autory (LaskaKit, KRtkovo.eu, Jakub Čížek) najdeš [na konci README](#-credits--pod%C4%9Bkov%C3%A1n%C3%AD). Bez nich by tenhle projekt nevznikl. 🙏

Náhled ovládání mapy: https://www.youtube.com/watch?v=hC3fB_leQMU

---

## ✨ Co přidává tento fork

- 🚒 **Herní režim „Zachraň město"** dostupný přímo z webového rozhraní (tlačítko *Zachraň město (hra)*), s návratem zpět na mapu.
- ❓ **100 zabudovaných kvízových otázek o ČR** na úrovni cca 10letého dítěte — hra je hratelná ihned po zapnutí, bez nahrávání (otázky lze i dočasně přepsat přes web).
- 🎨 **Decentně učesané webové rozhraní** mapy (příjemnější písmo, jemně stylovaná tlačítka, zvýrazněný aktivní režim).

## 🗺️ Režimy zobrazení na mapě

Přepínají se v prohlížeči na IP adrese mapy:

- aktuální **srážky** (radar)
- **teplotní** mapa, **vlhkost**, **tlak**, **prašnost** (data z čidel TMEP)
- **vlajka ČR** (zobrazí se i po startu)
- **krajská města**
- **kraje**
- 🚒 **Zachraň město** (hra — viz níže)

## 🚒 Herní režim „Zachraň město"

Klasická hra z dílny LaskaKit, tady integrovaná do webové appky. Na mapě se rozhoří 10 náhodných měst a ty je zachraňuješ správnými odpověďmi:

1. V ovládání mapy klikni na **🚒 Zachraň město (hra)**.
2. Odpovídej na otázky — **správná odpověď** město uhasí (zmodrá), **špatná** zapálí další město.
3. Tlačítkem **⬅ Zpět na mapu** se vrátíš k zobrazení dat.

### Vlastní otázky (volitelné)

Na herní stránce lze nahrát vlastní sadu otázek (dočasně přepíše zabudovanou, do restartu). Formát — jeden řádek = jedna otázka:

```
číslo;otázka;odpověď0;odpověď1;odpověď2;odpověď3;index_správné_odpovědi(0-3)
```

Příklad:

```
1;Hlavní město ČR?;Praha;Brno;Ostrava;Plzeň;0
```

Po restartu se vždy vrátí výchozích 100 zabudovaných otázek.

---

## 🔧 Instalace

Deska má vlastní USB-C programátor — stačí připojit kabel, žádná tlačítka se nemačkají.

### Knihovny

- [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel)
- [ArduinoJson](https://arduinojson.org/)

### Wi-Fi

V souboru **`WiFi_Config.h`** vyplň své SSID a heslo:

```cpp
const char *ssid  = "TVOJE_WIFI";
const char *heslo = "TVOJE_HESLO";
```

> `WiFi_Config.h` je ve verzovaném repu jen s placeholdery `changeme` — **necommituj do něj své skutečné heslo.**

### Nahrání — Arduino IDE

Návod na instalaci ESP32 desek do Arduino IDE: https://navody.dratek.cz/navody-k-produktum/jednoducha-instalace-esp32-do-arduino-ide.html

Vyber desku **ESP32 Dev Module**, správný port a klikni *Nahrát*.

### Nahrání — arduino-cli (alternativa z příkazové řádky)

```bash
# jednorázově: jádro + knihovny
arduino-cli core install esp32:esp32
arduino-cli lib install "Adafruit NeoPixel" "ArduinoJson"

# kompilace a nahrání (uprav port dle svého systému, např. /dev/ttyACM0)
arduino-cli compile --fqbn esp32:esp32:esp32 .
arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32 .
```

Na Linuxu je potřeba mít přístup k sériovému portu (skupina `dialout`):
```bash
sudo usermod -aG dialout $USER   # poté se odhlas a přihlas
```

## ▶️ Použití

Po nahrání otevři **Sériový monitor** (115200 Bd). Mapa po připojení k Wi-Fi vypíše svou **IP adresu** a hostname `laskakitmapa`. Tuto IP (nebo `http://laskakitmapa`) zadej v prohlížeči telefonu/počítače ve stejné síti — objeví se ovládání mapy včetně tlačítka pro hru.

---

## 🙏 Credits & poděkování

Tento projekt stojí na práci a hardwaru dalších autorů — velký dík patří:

- **[LaskaKit](https://www.laskakit.cz/)** — návrh a výroba **[Interaktivní mapy ČR (WS2812B)](https://www.laskakit.cz/laskakit-interaktivni-mapa-cr-ws2812b/)** a původní ukázkové firmwary v repu **[LaskaKit/LED_Czech_Map](https://github.com/LaskaKit/LED_Czech_Map)**.
- **Hra „Zachraň město"** pochází z LaskaKit repozitáře **[SW/Hra_Zachran_Mesto](https://github.com/LaskaKit/LED_Czech_Map/tree/main/SW/Hra_Zachran_Mesto)** — v tomto forku je adaptovaná a integrovaná do webové appky.
- **Ondřej Kotas ([KRtkovo.eu](https://krtkovo.eu/))** — autor firmwaru **[KRtekTM/Laskakit_MapaCR](https://github.com/KRtekTM/Laskakit_MapaCR)** s webovým přepínáním režimů mapy, ze kterého tento fork vychází.
- **[Jakub Čížek](https://www.zive.cz/)** (Živě.cz) — původní [srážkový radar pro mapu](https://github.com/jakubcizek/pojdmeprogramovatelektroniku/tree/master/SrazkovyRadar) ([článek na Živě.cz](https://www.zive.cz/clanky/naprogramovali-jsme-radarovou-mapu-ceska-ukaze-kde-prave-prsi-a-muzete-si-ji-dat-i-na-zed/sc-3-a-222111/default.aspx)), na kterém KRtekův firmware staví.

Respektujte prosím licence a autorství původních projektů.

---

## 📎 Poznámky

Data TMEP (JSON): https://wiki.tmep.cz/doku.php?id=ruzne:led_mapa_okresu_cr

Pořadí LED a odpovídající okresy:

| Real LaskaKit ID | TMEP ID | Okres |
|:--:|:-------:|:------|
| 24 | 1 | Cheb |
| 19 | 2 | Sokolov |
| 16 | 3 | **Karlovy Vary** |
| 10 | 4 | Chomutov |
| 15 | 5 | Louny |
| 9 | 6 | Most |
| 6 | 7 | Teplice |
| 8 | 8 | Litoměřice |
| 3 | 9 | **Ústí nad Labem** |
| 0 | 10 | Děčín |
| 4 | 11 | Česká Lípa |
| 1 | 12 | **Liberec** |
| 2 | 13 | Jablonec nad Nisou |
| 5 | 14 | Semily |
| 11 | 15 | Jičín |
| 7 | 16 | Trutnov |
| 12 | 17 | Náchod |
| 18 | 18 | **Hradec Králové** |
| 21 | 19 | Rychnov nad Kněžnou |
| 29 | 20 | Ústí nad Orlicí |
| 27 | 21 | **Pardubice** |
| 34 | 22 | Chrudim |
| 38 | 23 | Svitavy |
| 31 | 24 | Šumperk |
| 17 | 25 | Jeseník |
| 25 | 26 | Bruntál |
| 45 | 27 | **Olomouc** |
| 30 | 28 | Opava |
| 36 | 29 | **Ostrava-město** |
| 35 | 30 | Karviná |
| 42 | 31 | Frýdek-Místek |
| 44 | 32 | Nový Jičín |
| 56 | 33 | Vsetín |
| 48 | 34 | Přerov |
| 61 | 35 | **Zlín** |
| 57 | 36 | Kroměříž |
| 65 | 37 | Uherské Hradiště |
| 68 | 38 | Hodonín |
| 59 | 39 | Vyškov |
| 49 | 40 | Prostějov |
| 55 | 41 | Blansko |
| 63 | 42 | **Brno-město** |
| 71 | 44 | Břeclav |
| 69 | 45 | Znojmo |
| 62 | 46 | Třebíč |
| 47 | 47 | Žďár nad Sázavou |
| 53 | 48 | **Jihlava** |
| 46 | 49 | Havlíčkův Brod |
| 51 | 50 | Pelhřimov |
| 64 | 51 | Jindřichův Hradec |
| 52 | 52 | Tábor |
| 67 | 53 | **České Budějovice** |
| 70 | 54 | Český Krumlov |
| 66 | 55 | Prachatice |
| 60 | 56 | Strakonice |
| 58 | 57 | Písek |
| 54 | 58 | Klatovy |
| 50 | 59 | Domažlice |
| 37 | 60 | Tachov |
| 40 | 62 | **Plzeň-město** |
| 41 | 64 | Rokycany |
| 23 | 65 | Rakovník |
| 22 | 66 | Kladno |
| 14 | 67 | Mělník |
| 13 | 68 | Mladá Boleslav |
| 20 | 69 | Nymburk |
| 28 | 70 | Kolín |
| 33 | 71 | Kutná Hora |
| 39 | 72 | Benešov |
| 43 | 73 | Příbram |
| 32 | 74 | Beroun |
| 26 | 77 | **Praha** |
