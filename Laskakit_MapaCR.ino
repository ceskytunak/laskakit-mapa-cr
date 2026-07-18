/*
* Firmware pro LaskaKit Mapu ČR
* -----------------------------
* Autor: Ondřej Kotas, KRtkovo.eu
* Verze: 0.2
* https://github.com/KRtekTM/Laskakit_MapaCR
*
* Postaveno na základě kódu od Jakuba Čížka: https://github.com/jakubcizek/pojdmeprogramovatelektroniku/tree/master/SrazkovyRadar
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <vector>

// Customizable variables
#define DEBUG false
#include "WiFi_Config.h"
const char *hostname = "laskakitmapa";
uint32_t t, last24HourTaskTime = 0;  // Refreshovaci timestamp
uint32_t delay10 = 30000;            // Prodleva mezi aktualizaci dat, 30 vterin
uint32_t startupDelay = 10000;       // Zobraz vlajku na 10 vterin
uint8_t jas = 5;                     // Vychozi jas

// URL endpoints
const char *urlFlag = "https://raw.githubusercontent.com/KRtekTM/Laskakit_MapaCR/master/static/vlajkaCR.json";
const char *urlRain = "http://oracle-ams.kloboukuv.cloud/radarmapa/?chcu=posledni.json";
const char *urlTemp = "http://cdn.tmep.cz/app/export/okresy-cr-teplota.json";
const char *urlCitiesMajor = "https://raw.githubusercontent.com/KRtekTM/Laskakit_MapaCR/master/static/krajskaMesta.json";
const char *urlRegions = "https://raw.githubusercontent.com/KRtekTM/Laskakit_MapaCR/master/static/kraje.json";
const char *urlHumid = "http://cdn.tmep.cz/app/export/okresy-cr-vlhkost.json";
const char *urlPressure = "http://cdn.tmep.cz/app/export/okresy-cr-tlak.json";
const char *urlDust = "http://cdn.tmep.cz/app/export/okresy-cr-prasnost.json";

// Objekt pro ovladani adresovatelnych RGB LED
// Je jich 72 a jsou v serii pripojene na GPIO pin 25
Adafruit_NeoPixel pixely(72, 25, NEO_GRB + NEO_KHZ800);
// HTTP server bezici na standardnim TCP portu 80
WebServer server(80);
// Pamet pro JSON s povely
// Alokujeme pro nej 10 000 B, co je hodne,
// ale melo by to stacit i pro jSON,
// ktery bude obsahovat instrukce pro vsech 72 RGB LED
StaticJsonDocument<10000> doc;


// TMEP city ID mapping to correct LED id
int XX = -1;
int LEDsTMEP[77] = {
  24, 19, 16, 10, 15, 9, 6, 8, 3,
  0, 4, 1, 2, 5, 11, 7, 12, 18, 21,
  29, 27, 34, 38, 31, 17, 25, 45, 30, 36,
  35, 42, 44, 56, 48, 61, 57, 65, 68, 59,
  49, 55, 63, XX, 71, 69, 62, 47, 53, 46,
  51, 64, 52, 67, 70, 66, 60, 58, 54, 50,
  37, XX, 40, XX, 41, 23, 22, 14, 13, 20,
  28, 33, 39, 43, 32, XX, XX, 26
};


// Map selector
float maxThreshold, minThreshold;
enum SelectedMap {
  MapRain,
  MapTemp,
  MapFlag,
  MapCitiesMajor,
  MapRegions,
  MapHumid,
  MapPressure,
  MapDust
};
SelectedMap currentMap;
bool currentMapTMEP = false;
bool firstRun = true;

// ---------------------------------------------------------------------------
// Herní režim "Zachraň město"
// Integrováno z LaskaKit/LED_Czech_Map (SW/Hra_Zachran_Mesto).
// Používá stejný pásek RGB LED (pixely) i stejný WebServer (server).
// ---------------------------------------------------------------------------
#define GAME_BRIGHTNESS 50         // Herní režim svítí jasněji než mapa
bool gameActive = false;           // true = mapa je přepnuta do herního režimu
bool gameStarted = false;          // true = hra byla inicializována (hoří města)

enum CityState { CITY_OFF = 0, CITY_BURNING = 1, CITY_EXTINGUISHED = 2 };
CityState cityState[72];
int currentQuestion[72];           // -1 = pro město není vybrána otázka

// Názvy měst podle pořadí LaskaKit ID (LED 0-71)
String cityNames[72] = {
  "Děčín", "Liberec", "Jablonec nad Nisou", "Ústí nad Labem", "Česká Lípa", "Semily",
  "Teplice", "Trutnov", "Litoměřice", "Most", "Chomutov", "Jičín", "Náchod", "Mladá Boleslav",
  "Mělník", "Louny", "Karlovy Vary", "Jeseník", "Hradec Králové", "Sokolov", "Nymburk",
  "Rychnov nad Kněžnou", "Kladno", "Rakovník", "Cheb", "Bruntál", "Praha", "Pardubice",
  "Kolín", "Ústí nad Orlicí", "Opava", "Šumperk", "Beroun", "Kutná Hora", "Chrudim",
  "Karviná", "Ostrava-město", "Tachov", "Svitavy", "Benešov", "Plzeň-město", "Rokycany",
  "Frýdek-Místek", "Příbram", "Nový Jičín", "Olomouc", "Havlíčkův Brod", "Žďár nad Sázavou",
  "Prostějov", "Přerov", "Domažlice", "Pelhřimov", "Tábor", "Jihlava", "Klatovy", "Blansko",
  "Vsetín", "Kroměříž", "Písek", "Vyškov", "Strakonice", "Zlín", "Třebíč", "Brno-město",
  "Jindřichův Hradec", "Uherské Hradiště", "Prachatice", "České Budějovice", "Hodonín",
  "Znojmo", "Český Krumlov", "Břeclav"
};

struct Question {
  String q;
  String opts[4];
  uint8_t correct;
};
std::vector<Question> questionsDynamic;

// Dekoder JSONu a rozsvecovac svetylek
int jsonDecoder(String s, bool log) {
  DeserializationError e = deserializeJson(doc, s);
  if (e) {
    if (e == DeserializationError::InvalidInput) {
      return -1;
    } else if (e == DeserializationError::NoMemory) {
      return -2;
    } else {
      return -3;
    }
  } else {
    if (!(firstRun && currentMapTMEP)) pixely.clear();

    // Temperature map is in different format which needs remaping locations, follow else path
    if (!currentMapTMEP && (!(firstRun && currentMapTMEP))) {
      JsonArray mesta = doc["seznam"].as<JsonArray>();
      for (JsonObject mesto : mesta) {
        int id = mesto["id"];
        int r = mesto["r"];
        int g = mesto["g"];
        int b = mesto["b"];
        if (log) Serial.printf("Rozsvecuji mesto %d barvou R=%d G=%d B=%d\r\n", id, r, g, b);
        pixely.setPixelColor(id, pixely.Color(r, g, b));
      }
    } else {
      if (log) Serial.printf("minThreshold %f and maxThreshold %f\r\n", minThreshold, maxThreshold);

      // Read all TMEP districts with their indexes
      for (JsonObject item : doc.as<JsonArray>()) {
        int TMEPdistrictIndex = item["id"];
        // Substract 1, so index will start from 0
        TMEPdistrictIndex -= 1;
        float tempCelsius = item["h"];

        // Adjust the range of temperatures
        if (tempCelsius < minThreshold) minThreshold = tempCelsius;
        if (tempCelsius > maxThreshold) maxThreshold = tempCelsius;
        if (log) Serial.printf("minThreshold %f and maxThreshold %f\r\n", minThreshold, maxThreshold);

        if (log) Serial.printf("Mesto %d ma teplotu %f\r\n", LEDsTMEP[TMEPdistrictIndex], tempCelsius);

        if(!(firstRun && currentMapTMEP)) {
          byte r, g, b = 0;
          float ratio = 2 * (tempCelsius - minThreshold) / (maxThreshold - minThreshold);
          b = int(maxFloat(0.0, 255 * (1 - ratio)));
          r = int(maxFloat(0.0, 255 * (ratio - 1)));
          g = 255 - b - r;

          if (LEDsTMEP[TMEPdistrictIndex] >= 0) {
            if (log) Serial.printf("Rozsvecuji mesto %d barvou R=%d G=%d B=%d\r\n", LEDsTMEP[TMEPdistrictIndex], r, g, b);
            pixely.setPixelColor(LEDsTMEP[TMEPdistrictIndex], pixely.Color(r, g, b));
          }
        }
      }
    }

    if (!(firstRun && currentMapTMEP)) pixely.show();
    return 0;
  }
}

// Helper function to compare two float values and return the maximum
float maxFloat(float a, float b) {
  return (a > b) ? a : b;
}

// Stazeni radarovych dat z webu
void stahniData() {
  HTTPClient http;

  // Handle all maps with correct json format
  if (DEBUG) Serial.print("Zvolený mapový režim: ");
  if (DEBUG) Serial.println(GetSelectedMapMode());
  String url = "";
  switch (currentMap) {
    case MapFlag:
      url = urlFlag;
      currentMapTMEP = false;
      break;
    case MapCitiesMajor:
      url = urlCitiesMajor;
      currentMapTMEP = false;
      break;
    case MapRain:
      url = urlRain;
      currentMapTMEP = false;
      break;
    case MapTemp:
      url = urlTemp;
      currentMapTMEP = true;
      break;
    case MapRegions:
      url = urlRegions;
      currentMapTMEP = false;
      break;
    case MapHumid:
      url = urlHumid;
      currentMapTMEP = true;
      break;
    case MapPressure:
      url = urlPressure;
      currentMapTMEP = true;
      break;
    case MapDust:
      url = urlDust;
      currentMapTMEP = true;
      break;
    default:
      return;
  }

  if (DEBUG) Serial.print("Stahuji data z ");
  if (DEBUG) Serial.println(url);
  http.begin(url);
  http.addHeader("Cache-Control", "no-cache");
  http.addHeader("Pragma", "no-cache");
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int err = jsonDecoder(http.getString(), DEBUG);
    if (DEBUG) {
      switch (err) {
        case 0:
          Serial.println("Hotovo!");
          break;
        case -1:
          Serial.println("Spatny format JSONu");
          break;
        case -2:
          Serial.println("Malo pameti, navys velikost StaticJsonDocument");
          break;
        case -3:
          Serial.println("Chyba pri parsovani JSONu");
          break;
      }
    }
  }
  http.end();
}

String GetSelectedMapMode() {
  switch (currentMap) {
    case MapFlag:
      return "Vlajka";
    case MapCitiesMajor:
      return "Krajská města";
    case MapRain:
      return "Srážky";
    case MapTemp:
      return "Teplotní mapa";
    case MapRegions:
      return "Kraje";
    case MapHumid:
      return "Vlhkost";
    case MapPressure:
      return "Tlak";
    case MapDust:
      return "Prašnost";
    default:
      return "";
  }
}

String GetUnitForMapMode(SelectedMap mapToCheck) {
  switch (mapToCheck) {
    case MapTemp:
      return " °C";
    case MapHumid:
      return " %";
    case MapPressure:
      return " hPa";
    case MapDust:
      return " mg/m3";
    default:
      return "";
  }
}

void processMapRequest(const String &arg, SelectedMap mapType, bool selectedIsTMEP) {
  int err = jsonDecoder(arg, DEBUG);

  switch (err) {
    case 0:
      // Výběr mapy vždy ukončí herní režim a vrátí mapový jas
      if (gameActive) {
        gameActive = false;
        pixely.setBrightness(jas);
      }
      currentMap = mapType;
      currentMapTMEP = selectedIsTMEP;

      // Range adjusting
      if (currentMapTMEP) {
        firstRun = true;
        delay(100);
        stahniData();
        delay(100);
        firstRun = false;
      }

      // Redraw map
      stahniData();

      server.sendHeader("Location", "http://" + WiFi.localIP().toString() + "");
      server.send(302);  // Kód 302 označuje přesměrování (Found/Temporary Redirect)
      break;
    case -1:
      server.send(200, "text/plain", "CHYBA\nSpatny format JSON");
      break;
    case -2:
      server.send(200, "text/plain", "CHYBA\nMalo pameti RAM pro JSON. Navys ji!");
      break;
    case -3:
      server.send(200, "text/plain", "CHYBA\nNepodarilo se mi dekodovat jSON");
      break;
  }
}

float getMiddleNumber(float minVal, float maxVal) {
  return minVal + (maxVal - minVal) / 2;
}

// ---------------------------------------------------------------------------
// Herní logika "Zachraň město"
// ---------------------------------------------------------------------------

// JSON-escape textu (uvozovky, zpětné lomítko, nový řádek)
String gameJsonEscape(const String &s) {
  String out;
  out.reserve(s.length() * 2);
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s.charAt(i);
    if (c == '\\') out += "\\\\";
    else if (c == '"') out += "\\\"";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') { /* ignore */ }
    else out += c;
  }
  return out;
}

// Vybere náhodné dosud nehořící a neuhašené město
int gamePickRandomUnburned() {
  int candidates[72];
  int cnt = 0;
  for (int i = 0; i < 72; i++)
    if (cityState[i] == CITY_OFF) candidates[cnt++] = i;
  if (cnt == 0) return -1;
  return candidates[random(cnt)];
}

// Vykreslí LED podle herního stavu (statický obraz, bez blikání)
void gameUpdateStrip() {
  for (int i = 0; i < 72; i++) {
    if (cityState[i] == CITY_BURNING) pixely.setPixelColor(i, pixely.Color(255, 50, 0));
    else if (cityState[i] == CITY_EXTINGUISHED) pixely.setPixelColor(i, pixely.Color(0, 0, 255));
    else pixely.setPixelColor(i, 0);
  }
  pixely.setBrightness(GAME_BRIGHTNESS);
  pixely.show();
}

// Spustí (nebo restartuje) hru: zapálí 10 náhodných měst
void gameStart() {
  for (int i = 0; i < 72; i++) {
    cityState[i] = CITY_OFF;
    currentQuestion[i] = -1;
  }
  int count = 0;
  while (count < 10) {
    int idx = gamePickRandomUnburned();
    if (idx < 0) break;
    cityState[idx] = CITY_BURNING;
    count++;
  }
  gameStarted = true;
  gameUpdateStrip();
}

// Rozparsuje jeden řádek otázky: cislo;otazka;odp0;odp1;odp2;odp3;spravny(0-3)
bool gameParseQuestionLine(const String &lineRaw, Question &q, String &errorMsg) {
  String line = lineRaw;
  line.trim();
  if (line.length() == 0) { errorMsg = "Prázdný řádek"; return false; }

  int positions[6];
  int found = 0, pos = 0;
  while (found < 6) {
    int p = line.indexOf(';', pos);
    if (p < 0) break;
    positions[found++] = p;
    pos = p + 1;
  }
  if (found < 6) { errorMsg = "Nedostatek středníků (očekáváno 6 oddělovačů ';')."; return false; }

  String qtext    = line.substring(positions[0] + 1, positions[1]); qtext.trim();
  String o0       = line.substring(positions[1] + 1, positions[2]); o0.trim();
  String o1       = line.substring(positions[2] + 1, positions[3]); o1.trim();
  String o2       = line.substring(positions[3] + 1, positions[4]); o2.trim();
  String o3       = line.substring(positions[4] + 1, positions[5]); o3.trim();
  String correctS = line.substring(positions[5] + 1); correctS.trim();

  if (qtext.length() == 0) { errorMsg = "Prázdný text otázky."; return false; }
  if (o0.length() == 0 || o1.length() == 0 || o2.length() == 0 || o3.length() == 0) {
    errorMsg = "Některá odpověď je prázdná."; return false;
  }
  if (correctS.length() == 0) { errorMsg = "Chybí index správné odpovědi."; return false; }
  for (size_t i = 0; i < correctS.length(); ++i) {
    char c = correctS.charAt(i);
    if (!(c >= '0' && c <= '9')) { errorMsg = "Index správné odpovědi není číslo."; return false; }
  }
  int correct = correctS.toInt();
  if (correct < 0 || correct > 3) { errorMsg = "Index správné odpovědi mimo rozsah 0-3 (" + correctS + ")"; return false; }

  q.q = qtext;
  q.opts[0] = o0; q.opts[1] = o1; q.opts[2] = o2; q.opts[3] = o3;
  q.correct = (uint8_t)correct;
  return true;
}

// Načte více řádků otázek (CRLF i LF)
void gameLoadQuestions(const String &text, String &result) {
  questionsDynamic.clear();
  int start = 0, lineNo = 1;
  while (start < (int)text.length()) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = text.length();
    String line = text.substring(start, end);
    if (line.endsWith("\r")) line = line.substring(0, line.length() - 1);
    line.trim();
    if (line.length() > 0) {
      Question qq;
      String err;
      if (!gameParseQuestionLine(line, qq, err)) {
        result = "Chyba na řádku " + String(lineNo) + ": " + err;
        return;
      }
      questionsDynamic.push_back(qq);
    }
    start = end + 1;
    lineNo++;
  }
  result = "OK, načteno " + String(questionsDynamic.size()) + " otázek.";
}

// JSON se stavem všech měst
String gameStatusJSON() {
  String s = "{\"cities\":[";
  for (int i = 0; i < 72; i++) {
    s += "{\"idx\":" + String(i) + ",\"name\":\"" + gameJsonEscape(cityNames[i]) + "\",\"state\":" + String((int)cityState[i]) + "}";
    if (i < 71) s += ",";
  }
  s += "]}";
  return s;
}

// JSON otázky pro dané město
String gameQuestionJSON(int idx) {
  if (idx < 0 || idx >= 72) return String();
  if (questionsDynamic.empty()) {
    return "{\"city\":\"" + gameJsonEscape(cityNames[idx]) + "\",\"q\":{\"text\":\"(Žádné otázky)\",\"opts\":[\"-\",\"-\",\"-\",\"-\"]}}";
  }
  if (currentQuestion[idx] < 0 || currentQuestion[idx] >= (int)questionsDynamic.size()) {
    currentQuestion[idx] = random(questionsDynamic.size());
  }
  Question &q = questionsDynamic[currentQuestion[idx]];
  String s = "{\"city\":\"" + gameJsonEscape(cityNames[idx]) + "\",\"q\":{\"text\":\"" + gameJsonEscape(q.q) + "\",\"opts\":[";
  for (int i = 0; i < 4; i++) {
    s += "\"" + gameJsonEscape(q.opts[i]) + "\"";
    if (i < 3) s += ",";
  }
  s += "]}}";
  return s;
}

// Herní webová stránka
String gamePageHTML() {
  String s = R"rawliteral(<!doctype html>
<html lang="cs">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Zachraň město</title>
  <style>
    body{font-family:Arial,Helvetica,sans-serif;background:#f5f7fb;color:#222;margin:10px}
    h1{font-size:1.6rem;margin:0 0 12px 0}
    .card{background:#fff;border-radius:12px;padding:12px;box-shadow:0 4px 10px rgba(0,0,0,0.06);margin-bottom:12px}
    .city{display:flex;justify-content:space-between;padding:6px;border-bottom:1px solid #eee}
    .statusOff{color:#999}.statusBurn{color:#c0392b;font-weight:700}.statusExt{color:#2980b9;font-weight:700}
    button{display:block;width:100%;padding:10px 12px;margin:6px 0;border-radius:8px;border:0;background:#2d9cdb;color:white;cursor:pointer}
    .back{background:#7f8c8d}
    .question{margin:10px 0;font-weight:600}
    form textarea{width:100%;padding:8px;margin:6px 0;border-radius:8px;border:1px solid #ddd;box-sizing:border-box;font-family:monospace}
  </style>
</head>
<body>
  <h1>🚒 Zachraň město</h1>
  <button class="back" onclick="location.href='/gameStop'">⬅ Zpět na mapu</button>

  <div class="card"><h3>Otázka</h3><div id="questionBox">Klikni <b>Spustit hru</b>.</div></div>
  <div class="card"><h3>Hořící města</h3><div id="burningList"></div></div>
  <div class="card"><h3>Uhašená města</h3><div id="extList"></div></div>
  <div class="card"><h3>Bezpečná města</h3><div id="safeList"></div></div>

  <div class="card">
    <h3>Nahrát otázky</h3>
    <div style="font-size:0.9rem;color:#666;margin-bottom:6px">
      Každý řádek: <code>číslo;otázka;odp0;odp1;odp2;odp3;index_správné(0-3)</code>
    </div>
    <form onsubmit="uploadQuestions();return false;">
      <textarea id="questionsText" rows="6" placeholder="1;Hlavní město ČR?;Praha;Brno;Ostrava;Plzeň;0"></textarea>
      <button type="submit">Nahrát otázky</button>
    </form>
  </div>

  <div style="margin:12px 0">
    <button onclick="startGame()">Spustit hru</button>
    <button onclick="fetchStatus()">Aktualizovat</button>
  </div>

  <script>
    let currentCity=-1;
    async function fetchStatus(){
      const j=await (await fetch('/status')).json();
      const burn=document.getElementById('burningList');burn.innerHTML='';
      const ext=document.getElementById('extList');ext.innerHTML='';
      const safe=document.getElementById('safeList');safe.innerHTML='';
      j.cities.forEach((c)=>{
        const d=document.createElement('div');d.className='city';
        if(c.state==1){d.innerHTML='<div>'+c.name+'</div><div class=statusBurn>Hoří</div>';burn.appendChild(d);}
        else if(c.state==2){d.innerHTML='<div>'+c.name+'</div><div class=statusExt>Uhašeno</div>';ext.appendChild(d);}
        else{d.innerHTML='<div>'+c.name+'</div><div class=statusOff>OK</div>';safe.appendChild(d);}
      });
      const burning=j.cities.filter(c=>c.state==1);
      if(burning.length>0){currentCity=burning[0].idx;loadQuestion(currentCity);}
      else{document.getElementById('questionBox').innerHTML='<b>Všechna města uhašena! 🎉</b>';}
    }
    async function loadQuestion(cityIdx){
      const r=await fetch('/question?city='+cityIdx);
      if(!r.ok){document.getElementById('questionBox').innerHTML='<span style="color:red">Chyba: '+(await r.text())+'</span>';return;}
      const j=await r.json();
      const qb=document.getElementById('questionBox');
      qb.innerHTML='<b>Město: '+j.city+'</b><div class=question>'+j.q.text+'</div>';
      j.q.opts.forEach((o,i)=>{const b=document.createElement('button');b.textContent=o;b.onclick=()=>submitAnswer(cityIdx,i);qb.appendChild(b);});
    }
    async function submitAnswer(cityIdx,optIdx){
      const j=await (await fetch('/answer',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({city:cityIdx,answer:optIdx})})).json();
      if(j.error){alert("Chyba: "+j.error);return;}
      alert(j.correct?'Správně! Město uhašeno.':'Špatně! Další město hoří.');
      fetchStatus();
    }
    function startGame(){fetch('/start').then(()=>fetchStatus());}
    async function uploadQuestions(){
      const txt=document.getElementById('questionsText').value;
      if(!txt.trim()){alert("Pole je prázdné!");return;}
      const j=await (await fetch('/uploadQuestions',{method:'POST',headers:{'Content-Type':'text/plain'},body:txt})).json();
      if(j.error){alert("Chyba: "+j.error);}
      else{alert(j.ok);document.getElementById('questionsText').value='';fetchStatus();}
    }
    fetchStatus();
  </script>
</body>
</html>
)rawliteral";
  return s;
}

// --- Herní HTTP handlery ---
void handleGamePage() {
  gameActive = true;
  if (!gameStarted) gameStart();
  pixely.setBrightness(GAME_BRIGHTNESS);
  gameUpdateStrip();
  server.send(200, "text/html; charset=utf-8", gamePageHTML());
}

void handleGameStatus() {
  server.send(200, "application/json; charset=utf-8", gameStatusJSON());
}

void handleGameQuestion() {
  if (!server.hasArg("city")) { server.send(400, "application/json", "{\"error\":\"missing city\"}"); return; }
  int idx = server.arg("city").toInt();
  if (idx < 0 || idx >= 72) { server.send(400, "application/json", "{\"error\":\"bad city\"}"); return; }
  String payload = gameQuestionJSON(idx);
  if (payload.length() == 0) server.send(500, "application/json", "{\"error\":\"internal error\"}");
  else server.send(200, "application/json; charset=utf-8", payload);
}

void handleGameStart() {
  gameActive = true;
  gameStart();
  pixely.setBrightness(GAME_BRIGHTNESS);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleGameAnswer() {
  if (server.method() != HTTP_POST) { server.send(405); return; }
  String body = server.arg("plain");
  if (body.length() == 0) { server.send(400, "application/json", "{\"error\":\"empty body\"}"); return; }

  int city = -1, answer = -1;
  int pCity = body.indexOf("city");
  if (pCity >= 0) { int c = body.indexOf(':', pCity); if (c >= 0) city = body.substring(c + 1).toInt(); }
  int pAns = body.indexOf("answer");
  if (pAns >= 0) { int c = body.indexOf(':', pAns); if (c >= 0) answer = body.substring(c + 1).toInt(); }

  if (city < 0 || city >= 72 || answer < 0) { server.send(400, "application/json", "{\"error\":\"bad payload\"}"); return; }
  if (questionsDynamic.empty()) { server.send(400, "application/json", "{\"error\":\"no questions loaded\"}"); return; }

  int qidx = currentQuestion[city];
  if (qidx < 0 || qidx >= (int)questionsDynamic.size()) qidx = random(questionsDynamic.size());
  bool correct = (answer == questionsDynamic[qidx].correct);
  if (correct) {
    cityState[city] = CITY_EXTINGUISHED;
  } else {
    int nxt = gamePickRandomUnburned();
    if (nxt >= 0) cityState[nxt] = CITY_BURNING;
  }
  currentQuestion[city] = -1;
  gameUpdateStrip();
  server.send(200, "application/json", String("{\"correct\":") + (correct ? "true" : "false") + "}");
}

void handleGameUpload() {
  if (server.method() != HTTP_POST) { server.send(405); return; }
  String body = server.arg("plain");
  if (body.length() == 0) { server.send(400, "application/json", "{\"error\":\"empty body\"}"); return; }
  String result;
  gameLoadQuestions(body, result);
  if (result.startsWith("Chyba")) {
    server.send(400, "application/json", "{\"error\":\"" + gameJsonEscape(result) + "\"}");
  } else {
    server.send(200, "application/json", "{\"ok\":\"" + gameJsonEscape(result) + "\"}");
  }
}

// Ukončí hru a vrátí mapu do posledního mapového režimu
void handleGameStop() {
  gameActive = false;
  pixely.setBrightness(jas);
  stahniData();  // překreslí aktuální mapu
  server.sendHeader("Location", "http://" + WiFi.localIP().toString() + "");
  server.send(302);
}

// Tuto funkci HTTP server zavola v pripade HTTP GET/POST pzoadavku na korenovou cestu /
void httpDotaz(void) {
  // Pokud HTTP data obsahuji parametr mesta
  // predame jeho obsah JSON dekoderu
  if (server.hasArg("temp")) {
    maxThreshold = -25;
    minThreshold = 45;
    processMapRequest(server.arg("temp"), MapTemp, true);
  } else if (server.hasArg("rain")) {
    processMapRequest(server.arg("rain"), MapRain, false);
  } else if (server.hasArg("flag")) {
    processMapRequest(server.arg("flag"), MapFlag, false);
  } else if (server.hasArg("citiesMajor")) {
    processMapRequest(server.arg("citiesMajor"), MapCitiesMajor, false);
  } else if (server.hasArg("regions")) {
    processMapRequest(server.arg("regions"), MapRegions, false);
  } else if (server.hasArg("humidity")) {
    maxThreshold = 0;
    minThreshold = 100;
    processMapRequest(server.arg("humidity"), MapHumid, true);
  } else if (server.hasArg("pressure")) {
    maxThreshold = 850;
    minThreshold = 1200;
    processMapRequest(server.arg("pressure"), MapPressure, true);
  } else if (server.hasArg("dust")) {
    maxThreshold = 0;
    minThreshold = 300;
    processMapRequest(server.arg("dust"), MapDust, true);
  } else if (server.hasArg("css")) {
    String cssContent = String(
      "html, body {\n"
      "  text-align: center;\n"
      "  background: #FFFFFF;\n"
      "  color: #000000;\n"
      "}\n"
      "\n"
      "button {\n"
      "  width: 90%; height: 32px; margin: 6px;\n"
      "}\n"
      "\n"
      ".selected {\n"
      "  background: #00A2FF; font-weight: bold;\n"
      "}\n"
      ".minimum {\n"
      "  color: #00A2FF;\n"
      "}\n"
      ".maximum {\n"
      "  color: #FF0000;\n"
      "}\n"
      ".median {\n"
      "  color: #00FF00;\n"
      "}\n"
      "\n");
    server.send(200, "text/css", cssContent);
  }
  // Pokud jsme do mapy poslali jen HTTP GET/POST parametr smazat, mapa zhasne
  else if (server.hasArg("smazat")) {
    server.send(200, "text/plain", "OK");
    pixely.clear();
    pixely.show();
  } else if (server.hasArg("jas")) {
    server.send(200, "text/plain", "OK");
    jas = server.arg("jas").toInt();
    pixely.setBrightness(jas);
    pixely.show();
  }
  // Ve vsech ostatnich pripadech odpovime chybovym hlasenim
  else {
    if (firstRun) {
      server.send(200, "text/plain", "Startovani, cekej prosim.");
    } else {
      String htmlContent = String("<!DOCTYPE html>\n"
                                  "<html>\n"
                                  "<head>\n"
                                  "  <meta charset=\"UTF-8\">\n"
                                  "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=0\">\n"
                                  "  <title>Ovládání mapy</title>\n"
                                  "  <link href=\"http://"
                                  + WiFi.localIP().toString() + "?css={}\" rel=\"stylesheet\" crossorigin=\"anonymous\" />\n"
                                                                "</head>\n"
                                                                "<body>\n"
                                                                "  <h1>Ovládání mapy</h1>\n"
                                                                "  <h2>Zvolený režim: "
                                  + (GetSelectedMapMode()) + "</h2>\n"
                                  + (currentMapTMEP ? String("<p><span class=\"minimum\">Minimum: " + String(minThreshold) + GetUnitForMapMode(currentMap) + "</span><br />\n<span class=\"median\">" + String(getMiddleNumber(minThreshold, maxThreshold)) + GetUnitForMapMode(currentMap) + "</span><br />\n<span class=\"maximum\">Maximum: " + String(maxThreshold) + GetUnitForMapMode(currentMap) + "</span></p>\n") : String(""))
                                  + (currentMap == MapRain ? String("<p><span class=\"minimum\">Minimum: 0 mm/h</span><br />\n<span class=\"maximum\">Maximum: ∞ mm/h</span></p>\n") : String("")) + "\n"
                                  "  <button onclick=\"sendRainRequest()\" class=\""
                                  + (currentMap == MapRain ? String("selected") : String("")) + "\">Zobrazit srážky</button>\n"
                                                                                                "  <button onclick=\"sendTempRequest()\" class=\""
                                  + (currentMap == MapTemp ? String("selected") : String("")) + "\">Zobrazit teplotní mapu</button>\n"
                                                                                                "  <button onclick=\"sendHumidRequest()\" class=\""
                                  + (currentMap == MapHumid ? String("selected") : String("")) + "\">Zobrazit vlhkost</button>\n"
                                                                                                 "  <button onclick=\"sendPressureRequest()\" class=\""
                                  + (currentMap == MapPressure ? String("selected") : String("")) + "\">Zobrazit tlak</button>\n"
                                                                                                    "  <button onclick=\"sendDustRequest()\" class=\""
                                  + (currentMap == MapDust ? String("selected") : String("")) + "\">Zobrazit prašnost</button>\n"
                                                                                                "  <button onclick=\"sendFlagRequest()\" class=\""
                                  + (currentMap == MapFlag ? String("selected") : String("")) + "\">Zobrazit vlajku</button>\n"
                                                                                                "  <button onclick=\"sendCitiesMajorRequest()\" class=\""
                                  + (currentMap == MapCitiesMajor ? String("selected") : String("")) + "\">Zobrazit krajská města</button>\n"
                                                                                                       "  <button onclick=\"sendRegionsRequest()\" class=\""
                                  + (currentMap == MapRegions ? String("selected") : String("")) + "\">Zobrazit kraje</button>\n"
                                                                                                   "  <button onclick=\"location.href='/game'\">🚒 Zachraň město (hra)</button>\n"
                                                                                                   "\n"
                                                                                                   "  <script>\n"
                                                                                                   "    function sendRainRequest() {\n"
                                                                                                   "      var url = \"http://"
                                  + WiFi.localIP().toString() + "?rain={}\";\n"
                                                                "      window.location.href = url;\n"
                                                                "    }\n"
                                                                "\n"
                                                                "    function sendTempRequest() {\n"
                                                                "      var url = \"http://"
                                  + WiFi.localIP().toString() + "?temp={}\";\n"
                                                                "      window.location.href = url;\n"
                                                                "    }\n"
                                                                "\n"
                                                                "    function sendHumidRequest() {\n"
                                                                "      var url = \"http://"
                                  + WiFi.localIP().toString() + "?humidity={}\";\n"
                                                                "      window.location.href = url;\n"
                                                                "    }\n"
                                                                "\n"
                                                                "    function sendPressureRequest() {\n"
                                                                "      var url = \"http://"
                                  + WiFi.localIP().toString() + "?pressure={}\";\n"
                                                                "      window.location.href = url;\n"
                                                                "    }\n"
                                                                "\n"
                                                                "    function sendDustRequest() {\n"
                                                                "      var url = \"http://"
                                  + WiFi.localIP().toString() + "?dust={}\";\n"
                                                                "      window.location.href = url;\n"
                                                                "    }\n"
                                                                "\n"
                                                                "    function sendFlagRequest() {\n"
                                                                "      var url = \"http://"
                                  + WiFi.localIP().toString() + "?flag={}\";\n"
                                                                "      window.location.href = url;\n"
                                                                "    }\n"
                                                                "\n"
                                                                "    function sendCitiesMajorRequest() {\n"
                                                                "      var url = \"http://"
                                  + WiFi.localIP().toString() + "?citiesMajor={}\";\n"
                                                                "      window.location.href = url;\n"
                                                                "    }\n"
                                                                "\n"
                                                                "    function sendRegionsRequest() {\n"
                                                                "      var url = \"http://"
                                  + WiFi.localIP().toString() + "?regions={}\";\n"
                                                                "      window.location.href = url;\n"
                                                                "    }\n"
                                                                "  </script>\n"
                                                                "</body>\n"
                                                                "</html>\n");

      server.send(200, "text/html", htmlContent);
    }
  }
}

// Hlavni funkce setup se zpracuje hned po startu cipu ESP32
void setup() {
  // Nastartujeme serivou linku rychlosti 115200 b/s
  Serial.begin(115200);
  // Pripojime se k Wi-Fi a pote vypiseme do seriove linky IP adresu
  WiFi.disconnect();  // Vynucene odpojeni; obcas pomuze, kdyz se cip po startu nechce prihlasit
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(hostname);  //define hostname
  WiFi.begin(ssid, heslo);
  Serial.printf("Pripojuji se k %s ", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Automaticke pripojeni pri ztrate Wi-Fi
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  // Vypiseme do seriove linky pro kontrolu LAN IP adresu mapy
  Serial.print("OK\nIP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Hostname: ");
  Serial.println(hostname);
  // Pro HTTP pozadavku / zavolame funkci httpDotaz
  server.on("/", httpDotaz);
  // Herní režim "Zachraň město"
  server.on("/game", HTTP_GET, handleGamePage);
  server.on("/status", HTTP_GET, handleGameStatus);
  server.on("/question", HTTP_GET, handleGameQuestion);
  server.on("/start", HTTP_GET, handleGameStart);
  server.on("/answer", HTTP_POST, handleGameAnswer);
  server.on("/uploadQuestions", HTTP_POST, handleGameUpload);
  server.on("/gameStop", HTTP_GET, handleGameStop);
  // Seed pro náhodná čísla ve hře
  randomSeed(esp_random());
  // Inicializace herního stavu
  for (int i = 0; i < 72; i++) { cityState[i] = CITY_OFF; currentQuestion[i] = -1; }
  // Aktivujeme server
  server.begin();
  // Nakonfigurujeme adresovatelene LED do vychozi zhasnute pozice
  // Nastavime 8bit jas na hodnotu 5
  // Nebude svitit zbytecne moc a vyniknou mene kontrastni barvy
  pixely.begin();
  pixely.setBrightness(jas);
  pixely.clear();
  pixely.show();

  // Pri startovani zobraz vlajku
  currentMap = MapFlag;
  firstRun = true;
  currentMapTMEP = false;
  stahniData();
}

// Smycka loop se opakuje stale dokola
// a nastartuje se po zpracovani funkce setup
void loop() {
  // Vyridime pripadne TCP spojeni klientu se serverem
  server.handleClient();

  // Herní režim: animujeme hořící města a přeskočíme stahování mapy
  if (gameActive) {
    static unsigned long lastGameFrame = 0;
    if (millis() - lastGameFrame > 120) {
      lastGameFrame = millis();
      for (int i = 0; i < 72; i++) {
        if (cityState[i] == CITY_BURNING) {
          uint8_t r = 200 + random(0, 56);  // červená 200-255
          uint8_t g = random(0, 200);       // zelená 0-199
          pixely.setPixelColor(i, pixely.Color(r, g, 0));  // plápolání ohně
        } else if (cityState[i] == CITY_EXTINGUISHED) {
          pixely.setPixelColor(i, pixely.Color(0, 0, 150));  // stabilní modrá
        } else {
          pixely.setPixelColor(i, 0);  // zhasnuté město
        }
      }
      pixely.setBrightness(GAME_BRIGHTNESS);
      pixely.show();
    }
    delay(2);
    return;
  }

  // Jednou za zvoleny interval stahnu nova data
  if (millis() - t > ((firstRun && currentMap == MapFlag) ? startupDelay : delay10)) {
    if (firstRun) {
      currentMap = MapRain;
      currentMapTMEP = false;
      if(currentMapTMEP) stahniData();
      firstRun = false;
    }
    stahniData();
    t = millis();
  }

  // Přidáme část, která se vykoná jednou za 24 hodin
  // 86400000 = 24 * 60 * 60 * 1000
  if (millis() - last24HourTaskTime > 86400000) {
    // Vykonávání úlohy jednou za 24 hodin
    switch (currentMap) {
      case MapTemp:
        maxThreshold = -25;
        minThreshold = 45;
        break;
      case MapHumid:
        maxThreshold = 0;
        minThreshold = 100;
        break;
      case MapPressure:
        maxThreshold = 850;
        minThreshold = 1200;
        break;
      case MapDust:
        maxThreshold = 0;
        minThreshold = 300;
        break;
    }

    // For correct range adjustment after thresholds reset,
    // this will download data silently
    // (without glowing LEDs to not confuse viewer by different range)
    if (currentMapTMEP) {
      firstRun = true;
      stahniData();
      firstRun = false;
    }

    // Aktualizace času posledního vykonání úlohy
    last24HourTaskTime = millis();
  }

  // Pockame 2 ms (prenechame CPU pro ostatni ulohy na pozadi) a opakujeme
  delay(2);
}