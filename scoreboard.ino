#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// ======================================================
// Wi-Fi Configuration
// ======================================================
const char* WIFI_SSID = "Nick’s iPhone";
const char* WIFI_PASSWORD = "rainier776";

// ======================================================
// LED Panel Configuration
// ======================================================
#define PANEL_RES_X 32
#define PANEL_RES_Y 16
#define PANEL_CHAIN 2

const int16_t DISPLAY_WIDTH = PANEL_RES_X * PANEL_CHAIN;
const int16_t SCORE_X = 0;
const int16_t AT_BAT_X = PANEL_RES_X;
const int16_t AT_BAT_DISPLAY_X_OFFSET = 1;
const int16_t OUTS_DISPLAY_X_OFFSET = -1;

HUB75_I2S_CFG mxconfig(
  PANEL_RES_X,
  PANEL_RES_Y,
  PANEL_CHAIN
);

MatrixPanel_I2S_DMA *display = nullptr;

// ======================================================
// Web Server
// ======================================================
WebServer server(80);

// ======================================================
// Score State
// ======================================================
int homeScore = 0;
int awayScore = 0;
String homeName = "HOM";
String awayName = "AWY";
uint8_t homeColorR = 255;
uint8_t homeColorG = 255;
uint8_t homeColorB = 255;
uint8_t awayColorR = 255;
uint8_t awayColorG = 255;
uint8_t awayColorB = 255;
int balls = 0;
int strikes = 0;
int outs = 0;
int inning = 1;
bool topInning = true;

bool webAppConnected = false;
IPAddress connectedIp;
int16_t scrollX = DISPLAY_WIDTH;
unsigned long lastScrollFrame = 0;

const unsigned long SCROLL_FRAME_MS = 120;
const unsigned long STARTUP_SPLASH_MIN_MS = 10000;

// ======================================================
// Color helper
// ======================================================
uint16_t c(uint8_t r, uint8_t g, uint8_t b) {
  return display->color565(r, g, b);
}

String wifiEncryptionTypeName(wifi_auth_mode_t encryptionType) {
  switch (encryptionType) {
    case WIFI_AUTH_OPEN:
      return "OPEN";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2-ENTERPRISE";
    #ifdef WIFI_AUTH_WPA3_PSK
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3";
    #endif
    #ifdef WIFI_AUTH_WPA2_WPA3_PSK
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "WPA2/WPA3";
    #endif
    default:
      return "UNKNOWN";
  }
}

void scanWifiNetworks() {
  Serial.println("Scanning for Wi-Fi networks...");

  int networkCount = WiFi.scanNetworks();

  if (networkCount == WIFI_SCAN_FAILED) {
    Serial.println("Wi-Fi scan failed.");
    return;
  }

  if (networkCount == 0) {
    Serial.println("No Wi-Fi networks found.");
    return;
  }

  Serial.print(networkCount);
  Serial.println(" Wi-Fi network(s) found:");

  for (int i = 0; i < networkCount; i++) {
    Serial.print("  ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(WiFi.SSID(i));
    Serial.print(" | RSSI ");
    Serial.print(WiFi.RSSI(i));
    Serial.print(" dBm | channel ");
    Serial.print(WiFi.channel(i));
    Serial.print(" | auth ");
    Serial.print(wifiEncryptionTypeName(WiFi.encryptionType(i)));

    if (WiFi.SSID(i) == WIFI_SSID) {
      Serial.print(" <-- configured SSID");
    }

    Serial.println();
  }

  WiFi.scanDelete();
}

// ======================================================
// Display Helpers
// ======================================================
void drawCenteredSmallText(const String& line1, const String& line2 = "") {
  display->clearScreen();
  display->setTextSize(1);
  display->setTextColor(c(0, 180, 255));

  int16_t x1 = max(1, (DISPLAY_WIDTH - (int)line1.length() * 6) / 2);
  display->setCursor(x1, 0);
  display->print(line1);

  if (line2.length() > 0) {
    int16_t x2 = max(1, (DISPLAY_WIDTH - (int)line2.length() * 6) / 2);
    display->setCursor(x2, 8);
    display->print(line2);
  }
}

void drawCenteredTextLine(const String& text, int y, uint16_t color) {
  display->setTextColor(color);
  int16_t x = max(1, (DISPLAY_WIDTH - (int)text.length() * 6) / 2);
  display->setCursor(x, y);
  display->print(text);
}

void drawScrollingTextLine(const String& text, int y, uint16_t color, int16_t x) {
  display->setTextColor(color);
  if (x == 0) {
    x = -1;
  }
  display->setCursor(x, y);
  display->print(text);
}

String ipUrl(IPAddress ip) {
  String text = "http://";
  text += String(ip[0]);
  text += ".";
  text += String(ip[1]);
  text += ".";
  text += String(ip[2]);
  text += ".";
  text += String(ip[3]);
  return text;
}

bool advanceScroll(const String& text) {
  unsigned long now = millis();

  if (lastScrollFrame != 0 && now - lastScrollFrame < SCROLL_FRAME_MS) {
    return false;
  }

  lastScrollFrame = now;
  scrollX--;

  if (scrollX == 0) {
    scrollX = -1;
  }

  if (scrollX < -((int)text.length() * 6)) {
    scrollX = DISPLAY_WIDTH;
  }

  return true;
}

void drawWifiConnectingSplash() {
  const String brand = "GameChanger";
  unsigned long now = millis();

  bool scrollChanged = advanceScroll(brand);

  if (!scrollChanged) {
    return;
  }

  display->clearScreen();
  display->setTextSize(1);
  drawScrollingTextLine(brand, 0, c(0, 255, 0), scrollX);
  drawCenteredTextLine(((now / 1000) % 2 == 0) ? "SCORE" : "BOARD", 8, c(0, 180, 255));
}

void drawWaitingForWebApp() {
  String url = ipUrl(connectedIp);

  if (!advanceScroll(url)) {
    return;
  }

  display->clearScreen();
  display->setTextSize(1);
  drawCenteredTextLine("Visit", 0, c(0, 255, 0));
  drawScrollingTextLine(url, 8, c(0, 180, 255), scrollX);
}

void drawScoreRow(const String& teamName, int score, int y, uint16_t color, int16_t xOffset) {
  String name = teamName.substring(0, 3);
  String scoreText = String(score);
  int16_t scoreX = xOffset + max(19, PANEL_RES_X - (int)scoreText.length() * 6);

  display->setTextColor(color);
  display->setCursor(xOffset + 1, y);
  display->print(name);
  display->setCursor(scoreX, y);
  display->print(scoreText);
}

void drawScores(int16_t xOffset) {
  display->setTextSize(1);

  drawScoreRow(homeName, homeScore, 0, c(homeColorR, homeColorG, homeColorB), xOffset);
  drawScoreRow(awayName, awayScore, 8, c(awayColorR, awayColorG, awayColorB), xOffset);
}

void drawCountDot(int x, int y, uint16_t color) {
  display->drawPixel(x, y, color);
  display->drawPixel(x + 1, y, color);
  display->drawPixel(x, y + 1, color);
  display->drawPixel(x + 1, y + 1, color);
}

void drawDotGroup(int count, int x, int y, uint16_t color, int spacing = 4) {
  for (int i = 0; i < count; i++) {
    drawCountDot(x + (i * spacing), y, color);
  }
}

uint8_t tinyGlyphColumn(char ch, int col) {
  static const uint8_t digits[10][3] = {
    { 0b11111, 0b10001, 0b11111 },
    { 0b00000, 0b00000, 0b11111 },
    { 0b11101, 0b10101, 0b10111 },
    { 0b10101, 0b10101, 0b11111 },
    { 0b00111, 0b00100, 0b11111 },
    { 0b10111, 0b10101, 0b11101 },
    { 0b11111, 0b10101, 0b11101 },
    { 0b00001, 0b00001, 0b11111 },
    { 0b11111, 0b10101, 0b11111 },
    { 0b10111, 0b10101, 0b11111 }
  };

  if (ch >= '0' && ch <= '9') {
    return digits[ch - '0'][col];
  }

  switch (ch) {
    case 'o':
    case 'O': {
      const uint8_t glyph[3] = { 0b11111, 0b10001, 0b11111 };
      return glyph[col];
    }
    case 'u':
    case 'U': {
      const uint8_t glyph[3] = { 0b11111, 0b10000, 0b11111 };
      return glyph[col];
    }
    case 't':
    case 'T': {
      const uint8_t glyph[3] = { 0b00001, 0b11111, 0b00001 };
      return glyph[col];
    }
    case 's':
    case 'S': {
      const uint8_t glyph[3] = { 0b10111, 0b10101, 0b11101 };
      return glyph[col];
    }
    default:
      return 0;
  }
}

int tinyTextWidth(const String& text) {
  int width = 0;

  for (int i = 0; i < text.length(); i++) {
    width += text.charAt(i) == ' ' ? 2 : 4;
  }

  return max(0, width - 1);
}

void drawTinyText(const String& text, int x, int y, uint16_t color) {
  int cursorX = x;

  for (int i = 0; i < text.length(); i++) {
    char ch = text.charAt(i);

    if (ch == ' ') {
      cursorX += 2;
      continue;
    }

    for (int col = 0; col < 3; col++) {
      uint8_t column = tinyGlyphColumn(ch, col);

      for (int row = 0; row < 5; row++) {
        if (column & (1 << row)) {
          display->drawPixel(cursorX + col, y + row, color);
        }
      }
    }

    cursorX += 4;
  }
}

void drawInningArrow(int x, int y, bool pointingUp, uint16_t color) {
  if (pointingUp) {
    display->drawPixel(x + 2, y, color);
    display->drawPixel(x + 1, y + 1, color);
    display->drawPixel(x + 2, y + 1, color);
    display->drawPixel(x + 3, y + 1, color);
    display->drawFastHLine(x, y + 2, 5, color);
    display->drawPixel(x + 2, y + 3, color);
    display->drawPixel(x + 2, y + 4, color);
  } else {
    display->drawPixel(x + 2, y, color);
    display->drawPixel(x + 2, y + 1, color);
    display->drawFastHLine(x, y + 2, 5, color);
    display->drawPixel(x + 1, y + 3, color);
    display->drawPixel(x + 2, y + 3, color);
    display->drawPixel(x + 3, y + 3, color);
    display->drawPixel(x + 2, y + 4, color);
  }
}

void drawAtBat(int16_t xOffset) {
  xOffset += AT_BAT_DISPLAY_X_OFFSET;
  display->setTextSize(1);

  uint16_t ballsColor = c(0, 255, 0);
  uint16_t dashColor = c(255, 255, 255);
  uint16_t strikesColor = c(255, 0, 0);
  uint16_t outsColor = c(255, 0, 0);
  uint16_t inningColor = c(255, 255, 255);

  display->setCursor(xOffset + 0, 0);
  display->setTextColor(ballsColor);
  display->print(String(balls));
  display->setTextColor(dashColor);
  display->print("-");
  display->setTextColor(strikesColor);
  display->print(String(strikes));

  drawInningArrow(xOffset + 20, 1, topInning, inningColor);
  display->setTextColor(inningColor);

  if (inning < 10) {
    display->setCursor(xOffset + 26, 0);
    display->print(String(inning));
  } else {
    drawTinyText(String(inning), xOffset + 25, 1, inningColor);
  }

  String outsText = String(outs);
  outsText += " outs";
  int16_t outsX = xOffset + max(0, (PANEL_RES_X - tinyTextWidth(outsText)) / 2) + OUTS_DISPLAY_X_OFFSET;
  drawTinyText(outsText, outsX, 10, outsColor);
}

void drawScoreboard() {
  if (!webAppConnected && WiFi.status() == WL_CONNECTED) {
    drawWaitingForWebApp();
    return;
  }

  display->clearScreen();
  drawScores(SCORE_X);
  drawAtBat(AT_BAT_X);
}

// ======================================================
// File Serving Helpers
// ======================================================
String getContentType(const String& filename) {
  if (filename.endsWith(".html")) return "text/html";
  if (filename.endsWith(".css")) return "text/css";
  if (filename.endsWith(".js")) return "application/javascript";
  if (filename.endsWith(".json")) return "application/json";
  if (filename.endsWith(".png")) return "image/png";
  if (filename.endsWith(".jpg")) return "image/jpeg";
  if (filename.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}

bool serveFile(const String& path) {
  String filePath = path;

  if (filePath == "/") {
    filePath = "/index.html";
  }

  if (!LittleFS.exists(filePath)) {
    return false;
  }

  File file = LittleFS.open(filePath, "r");
  server.streamFile(file, getContentType(filePath));
  file.close();
  return true;
}

// ======================================================
// API Helpers
// ======================================================
char hexDigit(uint8_t value) {
  return value < 10 ? '0' + value : 'A' + (value - 10);
}

String colorToHex(uint8_t r, uint8_t g, uint8_t b) {
  String hex = "";
  hex += hexDigit(r >> 4);
  hex += hexDigit(r & 0x0F);
  hex += hexDigit(g >> 4);
  hex += hexDigit(g & 0x0F);
  hex += hexDigit(b >> 4);
  hex += hexDigit(b & 0x0F);
  return hex;
}

int hexValue(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  return -1;
}

bool parseHexColor(String value, uint8_t& r, uint8_t& g, uint8_t& b) {
  value.trim();

  if (value.startsWith("#")) {
    value = value.substring(1);
  }

  if (value.length() != 6) {
    return false;
  }

  int digits[6];

  for (int i = 0; i < 6; i++) {
    digits[i] = hexValue(value.charAt(i));

    if (digits[i] < 0) {
      return false;
    }
  }

  r = (digits[0] << 4) | digits[1];
  g = (digits[2] << 4) | digits[3];
  b = (digits[4] << 4) | digits[5];
  return true;
}

void sendScoreJson() {
  String json = "{";
  json += "\"home\":";
  json += String(homeScore);
  json += ",\"away\":";
  json += String(awayScore);
  json += ",\"homeName\":\"";
  json += homeName;
  json += "\",\"awayName\":\"";
  json += awayName;
  json += "\",\"balls\":";
  json += String(balls);
  json += ",\"strikes\":";
  json += String(strikes);
  json += ",\"outs\":";
  json += String(outs);
  json += ",\"inning\":";
  json += String(inning);
  json += ",\"inningHalf\":\"";
  json += topInning ? "top" : "bottom";
  json += "\"";
  json += ",\"homeColor\":\"#";
  json += colorToHex(homeColorR, homeColorG, homeColorB);
  json += "\",\"awayColor\":\"#";
  json += colorToHex(awayColorR, awayColorG, awayColorB);
  json += "\"";
  json += "}";

  server.send(200, "application/json", json);
}

String normalizeTeamName(const String& value, const String& fallback) {
  String normalized = value;
  normalized.trim();
  normalized.toUpperCase();

  String filtered = "";
  for (int i = 0; i < normalized.length(); i++) {
    char ch = normalized.charAt(i);

    if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
      filtered += ch;
    }
  }

  if (filtered.length() == 0) {
    filtered = fallback;
  }

  if (filtered.length() > 12) {
    filtered = filtered.substring(0, 12);
  }

  return filtered;
}

void handleTeamNames() {
  if (server.hasArg("home")) {
    homeName = normalizeTeamName(server.arg("home"), homeName);
  }

  if (server.hasArg("away")) {
    awayName = normalizeTeamName(server.arg("away"), awayName);
  }

  drawScoreboard();
  sendScoreJson();
}

void handleTeamColor(bool homeTeam) {
  if (!server.hasArg("color")) {
    server.send(400, "text/plain", "Missing color");
    return;
  }

  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;

  if (!parseHexColor(server.arg("color"), r, g, b)) {
    server.send(400, "text/plain", "Invalid color");
    return;
  }

  if (homeTeam) {
    homeColorR = r;
    homeColorG = g;
    homeColorB = b;
  } else {
    awayColorR = r;
    awayColorG = g;
    awayColorB = b;
  }

  drawScoreboard();
  sendScoreJson();
}

void handleHomeIncrement() {
  homeScore++;
  drawScoreboard();
  sendScoreJson();
}

void handleHomeDecrement() {
  if (homeScore > 0) {
    homeScore--;
  }
  drawScoreboard();
  sendScoreJson();
}

void handleAwayIncrement() {
  awayScore++;
  drawScoreboard();
  sendScoreJson();
}

void handleAwayDecrement() {
  if (awayScore > 0) {
    awayScore--;
  }
  drawScoreboard();
  sendScoreJson();
}

void handleReset() {
  homeScore = 0;
  awayScore = 0;
  drawScoreboard();
  sendScoreJson();
}

void handleAtBatReset() {
  balls = 0;
  strikes = 0;
  outs = 0;
  drawScoreboard();
  sendScoreJson();
}

void handleModeScore() {
  drawScoreboard();
  sendScoreJson();
}

void handleModeAtBat() {
  drawScoreboard();
  sendScoreJson();
}

void handleBallsIncrement() {
  if (balls < 3) {
    balls++;
  }
  drawScoreboard();
  sendScoreJson();
}

void handleBallsDecrement() {
  if (balls > 0) {
    balls--;
  }
  drawScoreboard();
  sendScoreJson();
}

void handleStrikesIncrement() {
  if (strikes < 2) {
    strikes++;
  }
  drawScoreboard();
  sendScoreJson();
}

void handleStrikesDecrement() {
  if (strikes > 0) {
    strikes--;
  }
  drawScoreboard();
  sendScoreJson();
}

void handleOutsIncrement() {
  if (outs < 2) {
    outs++;
  }
  drawScoreboard();
  sendScoreJson();
}

void handleOutsDecrement() {
  if (outs > 0) {
    outs--;
  }
  drawScoreboard();
  sendScoreJson();
}

void handleInningIncrement() {
  inning++;
  drawScoreboard();
  sendScoreJson();
}

void handleInningDecrement() {
  if (inning > 1) {
    inning--;
  }
  drawScoreboard();
  sendScoreJson();
}

void handleInningTop() {
  topInning = true;
  drawScoreboard();
  sendScoreJson();
}

void handleInningBottom() {
  topInning = false;
  drawScoreboard();
  sendScoreJson();
}

void handleWebAppConnect() {
  webAppConnected = true;
  drawScoreboard();
  sendScoreJson();
}

// ======================================================
// Setup Web Server Routes
// ======================================================
void setupServer() {
  server.on("/api/score", HTTP_GET, []() {
    sendScoreJson();
  });

  server.on("/api/connect", HTTP_POST, []() {
    handleWebAppConnect();
  });

  server.on("/api/home/increment", HTTP_POST, []() {
    handleHomeIncrement();
  });

  server.on("/api/home/decrement", HTTP_POST, []() {
    handleHomeDecrement();
  });

  server.on("/api/away/increment", HTTP_POST, []() {
    handleAwayIncrement();
  });

  server.on("/api/away/decrement", HTTP_POST, []() {
    handleAwayDecrement();
  });

  server.on("/api/reset", HTTP_POST, []() {
    handleReset();
  });

  server.on("/api/atbat/reset", HTTP_POST, []() {
    handleAtBatReset();
  });

  server.on("/api/teams", HTTP_POST, []() {
    handleTeamNames();
  });

  server.on("/api/home/color", HTTP_POST, []() {
    handleTeamColor(true);
  });

  server.on("/api/away/color", HTTP_POST, []() {
    handleTeamColor(false);
  });

  server.on("/api/mode/score", HTTP_POST, []() {
    handleModeScore();
  });

  server.on("/api/mode/atbat", HTTP_POST, []() {
    handleModeAtBat();
  });

  server.on("/api/balls/increment", HTTP_POST, []() {
    handleBallsIncrement();
  });

  server.on("/api/balls/decrement", HTTP_POST, []() {
    handleBallsDecrement();
  });

  server.on("/api/strikes/increment", HTTP_POST, []() {
    handleStrikesIncrement();
  });

  server.on("/api/strikes/decrement", HTTP_POST, []() {
    handleStrikesDecrement();
  });

  server.on("/api/outs/increment", HTTP_POST, []() {
    handleOutsIncrement();
  });

  server.on("/api/outs/decrement", HTTP_POST, []() {
    handleOutsDecrement();
  });

  server.on("/api/inning/increment", HTTP_POST, []() {
    handleInningIncrement();
  });

  server.on("/api/inning/decrement", HTTP_POST, []() {
    handleInningDecrement();
  });

  server.on("/api/inning/top", HTTP_POST, []() {
    handleInningTop();
  });

  server.on("/api/inning/bottom", HTTP_POST, []() {
    handleInningBottom();
  });

  // Static files from LittleFS
  server.onNotFound([]() {
    if (!serveFile(server.uri())) {
      server.send(404, "text/plain", "File not found");
    }
  });

  server.begin();
  Serial.println("Web server started.");
}

// ======================================================
// Wi-Fi
// ======================================================
void connectToWifi() {
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  scanWifiNetworks();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  scrollX = DISPLAY_WIDTH;
  lastScrollFrame = 0;

  unsigned long splashStart = millis();
  bool announcedConnected = false;

  while (WiFi.status() != WL_CONNECTED || millis() - splashStart < STARTUP_SPLASH_MIN_MS) {
    drawWifiConnectingSplash();
    delay(50);

    if (WiFi.status() == WL_CONNECTED) {
      if (!announcedConnected) {
        Serial.println();
        Serial.println("Wi-Fi connected. Holding startup splash.");
        announcedConnected = true;
      }
    } else {
      Serial.print(".");
    }
  }

  connectedIp = WiFi.localIP();
  scrollX = DISPLAY_WIDTH;
  lastScrollFrame = 0;

  Serial.println();
  Serial.print("IP address: ");
  Serial.println(connectedIp);

  drawWaitingForWebApp();
}

// ======================================================
// Arduino Setup / Loop
// ======================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  mxconfig.clkphase = false;
  display = new MatrixPanel_I2S_DMA(mxconfig);
  display->begin();
  display->setTextWrap(false);
  display->setBrightness8(120);
  display->clearScreen();

  drawCenteredSmallText("BOOT");

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed.");
    drawCenteredSmallText("FS", "ERR");
    return;
  }

  Serial.println("LittleFS mounted.");

  connectToWifi();

  setupServer();
}

void loop() {
  server.handleClient();

  if (!webAppConnected && WiFi.status() == WL_CONNECTED) {
    drawWaitingForWebApp();
  }
}
