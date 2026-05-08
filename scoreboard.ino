#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// ======================================================
// Wi-Fi Configuration
// ======================================================
const char* WIFI_SSID = "beast";
const char* WIFI_PASSWORD = "rainier776";

// ======================================================
// LED Panel Configuration
// ======================================================
#define PANEL_RES_X 32
#define PANEL_RES_Y 16
#define PANEL_CHAIN 1

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
int balls = 0;
int strikes = 0;
int outs = 0;
int inning = 1;

enum DisplayMode {
  MODE_SCORE,
  MODE_AT_BAT
};

DisplayMode displayMode = MODE_SCORE;

// ======================================================
// Color helper
// ======================================================
uint16_t c(uint8_t r, uint8_t g, uint8_t b) {
  return display->color565(r, g, b);
}

// ======================================================
// Display Helpers
// ======================================================
void drawCenteredSmallText(const String& line1, const String& line2 = "") {
  display->clearScreen();
  display->setTextSize(1);
  display->setTextColor(c(0, 180, 255));

  int16_t x1 = max(1, (32 - (int)line1.length() * 6) / 2);
  display->setCursor(x1, 0);
  display->print(line1);

  if (line2.length() > 0) {
    int16_t x2 = max(1, (32 - (int)line2.length() * 6) / 2);
    display->setCursor(x2, 8);
    display->print(line2);
  }
}

void drawScoreRow(const String& teamName, int score, int y, uint16_t color) {
  String name = teamName.substring(0, 3);
  String scoreText = String(score);
  int16_t scoreX = max(19, 32 - (int)scoreText.length() * 6);

  display->setTextColor(color);
  display->setCursor(1, y);
  display->print(name);
  display->setCursor(scoreX, y);
  display->print(scoreText);
}

void drawScores() {
  display->clearScreen();
  display->setTextSize(1);

  drawScoreRow(homeName, homeScore, 0, c(0, 255, 0));
  drawScoreRow(awayName, awayScore, 8, c(255, 0, 0));
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

void drawAtBat() {
  display->clearScreen();
  display->setTextSize(1);

  uint16_t ballsColor = c(0, 245, 160);
  uint16_t strikesColor = c(255, 190, 0);
  uint16_t outsColor = c(255, 70, 100);
  uint16_t inningColor = c(0, 180, 255);

  display->setTextColor(ballsColor);
  display->setCursor(1, 0);
  display->print("B");
  drawDotGroup(balls, 8, 3, ballsColor);

  display->setTextColor(strikesColor);
  display->setCursor(1, 8);
  display->print("S");
  drawDotGroup(strikes, 8, 11, strikesColor);

  display->setTextColor(outsColor);
  display->setCursor(20, 0);
  display->print("O");
  drawDotGroup(outs, 27, 3, outsColor, 3);

  String inningText = "I";
  inningText += String(inning);
  display->setTextColor(inningColor);
  display->setCursor(20, 8);
  display->print(inningText);
}

void drawCurrentMode() {
  if (displayMode == MODE_AT_BAT) {
    drawAtBat();
  } else {
    drawScores();
  }
}

void showIpFeedback(IPAddress ip) {
  // The full IP address is too wide for 32x16 pixels.
  // So we briefly show each octet.
  for (int i = 0; i < 4; i++) {
    drawCenteredSmallText("IP", String(ip[i]));
    delay(800);
  }

  drawCurrentMode();
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
  json += ",\"displayMode\":\"";
  json += (displayMode == MODE_AT_BAT ? "atBat" : "score");
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

  drawCurrentMode();
  sendScoreJson();
}

void handleHomeIncrement() {
  homeScore++;
  drawCurrentMode();
  sendScoreJson();
}

void handleHomeDecrement() {
  if (homeScore > 0) {
    homeScore--;
  }
  drawCurrentMode();
  sendScoreJson();
}

void handleAwayIncrement() {
  awayScore++;
  drawCurrentMode();
  sendScoreJson();
}

void handleAwayDecrement() {
  if (awayScore > 0) {
    awayScore--;
  }
  drawCurrentMode();
  sendScoreJson();
}

void handleReset() {
  homeScore = 0;
  awayScore = 0;
  drawCurrentMode();
  sendScoreJson();
}

void handleAtBatReset() {
  balls = 0;
  strikes = 0;
  outs = 0;
  drawCurrentMode();
  sendScoreJson();
}

void handleModeScore() {
  displayMode = MODE_SCORE;
  drawCurrentMode();
  sendScoreJson();
}

void handleModeAtBat() {
  displayMode = MODE_AT_BAT;
  drawCurrentMode();
  sendScoreJson();
}

void handleBallsIncrement() {
  if (balls < 3) {
    balls++;
  }
  drawCurrentMode();
  sendScoreJson();
}

void handleBallsDecrement() {
  if (balls > 0) {
    balls--;
  }
  drawCurrentMode();
  sendScoreJson();
}

void handleStrikesIncrement() {
  if (strikes < 2) {
    strikes++;
  }
  drawCurrentMode();
  sendScoreJson();
}

void handleStrikesDecrement() {
  if (strikes > 0) {
    strikes--;
  }
  drawCurrentMode();
  sendScoreJson();
}

void handleOutsIncrement() {
  if (outs < 2) {
    outs++;
  }
  drawCurrentMode();
  sendScoreJson();
}

void handleOutsDecrement() {
  if (outs > 0) {
    outs--;
  }
  drawCurrentMode();
  sendScoreJson();
}

void handleInningIncrement() {
  inning++;
  drawCurrentMode();
  sendScoreJson();
}

void handleInningDecrement() {
  if (inning > 1) {
    inning--;
  }
  drawCurrentMode();
  sendScoreJson();
}

// ======================================================
// Setup Web Server Routes
// ======================================================
void setupServer() {
  server.on("/api/score", HTTP_GET, []() {
    sendScoreJson();
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

  drawCenteredSmallText("WIFI", "...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    attempts++;

    if (attempts % 4 == 0) {
      drawCenteredSmallText("WIFI", String(attempts / 2) + "s");
    }
  }

  Serial.println();
  Serial.println("Wi-Fi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  showIpFeedback(WiFi.localIP());
}

// ======================================================
// Arduino Setup / Loop
// ======================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  display = new MatrixPanel_I2S_DMA(mxconfig);
  display->begin();
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

  drawCurrentMode();

  setupServer();
}

void loop() {
  server.handleClient();
}
