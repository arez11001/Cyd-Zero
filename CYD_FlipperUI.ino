/*
 * CYD ZERO - Bruce Firmware UI
 * Flipper Zero-style UI for ESP32-2432S028 (CYD)
 * Replicates ALL Bruce firmware features with Flipper aesthetic
 *
 * Libraries needed (Arduino IDE):
 *   - TFT_eSPI by Bodmer         (configure User_Setup.h - included)
 *   - XPT2046_Touchscreen by Paul Stoffregen
 *
 * Board: ESP32 Dev Module | 240MHz | 4MB flash | Default partition
 */

#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// ── Touchscreen SPI ──────────────────────────────────────────────────
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();

// ── Display ──────────────────────────────────────────────────────────
#define W        320
#define H        240
#define STATUS_H  14
#define TITLE_H   24
#define CONTENT_Y (STATUS_H + TITLE_H)
#define CONTENT_H (H - CONTENT_Y - 16)

// ── Flipper colour palette ────────────────────────────────────────────
#define FLP_ORANGE  0xFB60
#define FLP_BLACK   TFT_BLACK
#define FLP_WHITE   TFT_WHITE
#define FLP_DKGRAY  0x2104

// ── Touch calibration ────────────────────────────────────────────────
#define TOUCH_X_MIN  200
#define TOUCH_X_MAX  3800
#define TOUCH_Y_MIN  200
#define TOUCH_Y_MAX  3800

// ── Menu structure ───────────────────────────────────────────────────
struct MenuItem { const char* label; const char* icon; };

#define CAT_COUNT 9
const MenuItem categories[CAT_COUNT] = {
  {"WiFi",      "WF"},
  {"Bluetooth", "BT"},
  {"RFID/NFC",  "NF"},
  {"Infrared",  "IR"},
  {"Sub-GHz",   "RF"},
  {"BadUSB",    "BD"},
  {"GPS",       "GP"},
  {"Files",     "FL"},
  {"Settings",  "ST"},
};

#define MAX_SUB 12
const char* subWiFi[MAX_SUB]      = {"Scan Networks","Connect","Evil Portal","Deauth Attack","Probe Sniffer","Handshake Cap","Wardriving","WiFi Config","AP Mode","DHCP Starv","MAC Flood","Karma Attack"};
const char* subBluetooth[MAX_SUB] = {"BLE Scan","BT Spam","AppleJuice","Samsung Spam","Android Spam","Windows Spam","BLE Keyboard","Bad BLE","Brucegotchi","BLE Config","BT Sniff","iBeacon"};
const char* subRFID[MAX_SUB]      = {"HF Scan","HF Read","HF Write","HF Clone","HF Emulate","LF Scan","LF Read","LF Write","LF Clone","LF Emulate","NFC NDEF","SRIX Tool"};
const char* subIR[MAX_SUB]        = {"IR Read","Custom IR","IR Jammer","Spam All","TV Power","TV Mute","Vol Up","Vol Down","CH Up","CH Down","IR Config","Send File"};
const char* subSubGHz[MAX_SUB]    = {"Scan/Copy","Custom Tx","RF Replay","RAW Record","Spectrum","RSSI View","RF Config","Freq Scan","NRF Jammer","CH Jammer","LoRa Radio","FM Spectrum"};
const char* subBadUSB[MAX_SUB]    = {"Run Script","BLE HID","USB Keyboard","Ducky Typer","Run in BG","Presenter","Send Email","Serial USB","BadUSB/BLE","JS Interp","App Store","Custom HTML"};
const char* subGPS[MAX_SUB]       = {"GPS Tracker","GPS Config","GPS Pins","Wardriving","Location","NTP Sync","Wigle Upload","View Log","Set Time","Clock","",""};
const char* subFiles[MAX_SUB]     = {"SD Card","LittleFS","View File","Text Viewer","View Image","Copy File","Delete File","New File","New Folder","File Info","Screenshot","Recv File"};
const char* subSettings[MAX_SUB]  = {"Brightness","UI Theme","UI Color","Orientation","BLE Config","IR Config","RF Config","UART Pins","Device Info","About","Factory Reset","Deep Sleep"};

const char** subMenus[CAT_COUNT]  = {subWiFi,subBluetooth,subRFID,subIR,subSubGHz,subBadUSB,subGPS,subFiles,subSettings};
int subCounts[CAT_COUNT]          = {12,12,12,12,12,12,10,12,12};

// ── App state ─────────────────────────────────────────────────────────
enum State { ST_BOOT, ST_MAIN, ST_SUB, ST_ACTION };
State appState  = ST_BOOT;
int catPage     = 0;
int catSelected = 0;
int subScroll   = 0;
int subSelected = 0;
int bootFrame   = 0;
unsigned long lastFrameMs = 0;
unsigned long bootStart   = 0;

// ── Touch helper ─────────────────────────────────────────────────────
struct Touch { int x, y; bool valid; };
Touch getTouch() {
  Touch t = {0, 0, false};
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point p = ts.getPoint();
    t.x = constrain(map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, W-1), 0, W-1);
    t.y = constrain(map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, H-1), 0, H-1);
    t.valid = true;
    delay(60);
  }
  return t;
}

// ── Status bar ───────────────────────────────────────────────────────
void drawStatusBar(const char* title = nullptr) {
  tft.fillRect(0, 0, W, STATUS_H, FLP_BLACK);
  tft.fillRect(4, 11, 2, 2, FLP_ORANGE);
  tft.fillRect(7,  8, 2, 5, FLP_ORANGE);
  tft.fillRect(10, 5, 2, 8, FLP_ORANGE);
  tft.setTextColor(FLP_ORANGE); tft.setTextSize(1);
  tft.setCursor(15, 3); tft.print("B");
  tft.drawRect(W-22, 3, 18, 8, FLP_ORANGE);
  tft.fillRect(W-4,  5,  3, 4, FLP_ORANGE);
  tft.fillRect(W-20, 4, 14, 6, FLP_ORANGE);
  if (title) {
    int len = strlen(title) * 6;
    tft.setCursor((W - len) / 2, 3);
    tft.print(title);
  }
}

void drawTitleBar(const char* text) {
  tft.fillRect(0, STATUS_H, W, TITLE_H, FLP_ORANGE);
  tft.setTextColor(FLP_BLACK); tft.setTextSize(2);
  int len = strlen(text) * 12;
  tft.setCursor((W - len) / 2, STATUS_H + 4);
  tft.print(text);
}

void drawNavBar(const char* left = nullptr, const char* right = nullptr) {
  tft.fillRect(0, H-16, W, 16, FLP_BLACK);
  tft.setTextColor(FLP_ORANGE); tft.setTextSize(1);
  if (left)  { tft.setCursor(4, H-12); tft.print(left); }
  if (right) { tft.setCursor(W-strlen(right)*6-4, H-12); tft.print(right); }
}

// ═══════════════════════════════════════════════════════════════
// BOOT ANIMATION
// ═══════════════════════════════════════════════════════════════
void drawBoot(int frame) {
  tft.fillScreen(FLP_ORANGE);
  drawStatusBar("BOOTING");

  int bob       = (frame % 4 < 2) ? 0 : 1;
  int tailSwing = (frame % 4 < 2) ? -4 : 4;

  tft.fillRect(10, 185+bob, W-20, 3, FLP_BLACK);

  // VCR
  tft.fillRoundRect(20, 155+bob, 68, 14, 2, FLP_BLACK);
  for (int d = 0; d < 8; d++) tft.fillRect(23+d*7, 158+bob, 5, 8, FLP_ORANGE);

  // TV
  tft.fillRoundRect(22, 100+bob, 65, 55, 4, FLP_BLACK);
  tft.fillRect(27, 105+bob, 55, 38, FLP_ORANGE);
  for (int s = 0; s < 5; s++) tft.drawLine(29, 110+s*6+bob, 80, 110+s*6+bob, FLP_BLACK);
  tft.fillRect(29, 110+(frame%5)*6+bob, 52, 2, FLP_DKGRAY);
  tft.fillRect(30, 155+bob, 8, 6, FLP_BLACK);
  tft.fillRect(71, 155+bob, 8, 6, FLP_BLACK);
  tft.drawLine(40, 100+bob, 28, 78+bob, FLP_BLACK);
  tft.drawLine(40, 100+bob, 52, 78+bob, FLP_BLACK);

  // Couch
  tft.fillRoundRect(150, 110+bob, 145, 65, 6, FLP_BLACK);
  tft.fillRect(156, 145+bob, 133, 28, FLP_ORANGE);
  tft.fillRect(150, 120+bob, 14, 58, FLP_BLACK);
  tft.fillRect(281, 120+bob, 14, 58, FLP_BLACK);

  // Dolphin
  int dx = 155, dy = 80 + bob;
  tft.fillEllipse(dx+60, dy+45, 55, 28, FLP_BLACK);
  tft.fillCircle(dx+108, dy+33, 22, FLP_BLACK);
  tft.fillTriangle(dx+125, dy+33, dx+148, dy+27, dx+148, dy+40, FLP_BLACK);
  tft.fillCircle(dx+113, dy+27, 5, FLP_ORANGE);
  tft.fillCircle(dx+114, dy+27, 3, FLP_BLACK);
  tft.fillRect(dx+106, dy+24, 12, 6, FLP_BLACK);
  tft.fillRect(dx+119, dy+24, 12, 6, FLP_BLACK);
  tft.drawLine(dx+106, dy+26, dx+100, dy+25, FLP_BLACK);
  tft.drawLine(dx+130, dy+26, dx+136, dy+25, FLP_BLACK);
  tft.fillTriangle(dx+55, dy+17, dx+68, dy-5, dx+80, dy+17, FLP_BLACK);
  tft.fillTriangle(dx, dy+40+tailSwing, dx-18, dy+28, dx-18, dy+52+tailSwing, FLP_BLACK);
  tft.fillEllipse(dx+35, dy+60, 22, 9, FLP_BLACK);

  // Bowl + snacks
  int bx = dx+15, by = dy+68;
  tft.fillRect(bx, by, 32, 10, FLP_BLACK);
  tft.fillEllipse(bx+16, by+10, 18, 7, FLP_BLACK);
  tft.fillRect(bx+4, by-6, 24, 7, FLP_ORANGE);
  for (int p = 0; p < 5; p++) tft.fillCircle(bx+4+p*5, by-8, 2+((p+frame)%2), FLP_BLACK);

  // Branding
  tft.setTextColor(FLP_BLACK); tft.setTextSize(3);
  tft.setCursor(W/2-57, H-52); tft.print("CYD ZERO");
  tft.setTextSize(1);
  tft.setCursor(W/2-30, H-26); tft.print("Bruce FW UI");
  tft.fillRect(W/2-16, H-14, 36, 10, FLP_ORANGE);
  for (int d = 0; d < (frame%4)+1; d++)
    tft.fillCircle(W/2-8+d*9, H-9, 3, FLP_BLACK);
}

// ═══════════════════════════════════════════════════════════════
// MAIN MENU — 2x2 tile grid, paged with side arrows
// ═══════════════════════════════════════════════════════════════
#define TILES_PER_PAGE 4
#define TILE_W  110
#define TILE_H   80
#define TILE_GAP  8

void drawMainMenu() {
  tft.fillScreen(FLP_ORANGE);
  drawStatusBar("CYD ZERO");
  drawTitleBar("MAIN MENU");

  int totalPages = (CAT_COUNT + TILES_PER_PAGE - 1) / TILES_PER_PAGE;
  int startIdx   = catPage * TILES_PER_PAGE;
  int endIdx     = min(startIdx + TILES_PER_PAGE, CAT_COUNT);
  int count      = endIdx - startIdx;
  int cols       = 2;
  int gridW      = cols * TILE_W + (cols-1) * TILE_GAP;
  int gridX      = (W - gridW) / 2;
  int gridY      = CONTENT_Y + 6;

  for (int i = 0; i < count; i++) {
    int ci  = startIdx + i;
    int col = i % cols;
    int row = i / cols;
    int tx  = gridX + col * (TILE_W + TILE_GAP);
    int ty  = gridY + row * (TILE_H + TILE_GAP);
    bool sel = (ci == catSelected);

    if (sel) {
      tft.fillRoundRect(tx, ty, TILE_W, TILE_H, 8, FLP_BLACK);
      tft.setTextColor(FLP_ORANGE); tft.setTextSize(3);
      tft.setCursor(tx + (TILE_W - strlen(categories[ci].icon)*18)/2, ty+8);
      tft.print(categories[ci].icon);
      tft.setTextSize(1);
      tft.setCursor(tx + (TILE_W - strlen(categories[ci].label)*6)/2, ty+TILE_H-16);
      tft.print(categories[ci].label);
    } else {
      tft.fillRoundRect(tx, ty, TILE_W, TILE_H, 8, FLP_ORANGE);
      tft.drawRoundRect(tx, ty, TILE_W, TILE_H, 8, FLP_BLACK);
      tft.setTextColor(FLP_BLACK); tft.setTextSize(3);
      tft.setCursor(tx + (TILE_W - strlen(categories[ci].icon)*18)/2, ty+8);
      tft.print(categories[ci].icon);
      tft.setTextSize(1);
      tft.setCursor(tx + (TILE_W - strlen(categories[ci].label)*6)/2, ty+TILE_H-16);
      tft.print(categories[ci].label);
    }
  }

  // Left arrow
  uint16_t lc = (catPage > 0) ? FLP_BLACK : FLP_DKGRAY;
  tft.fillTriangle(10, H/2, 22, H/2-14, 22, H/2+14, lc);

  // Right arrow
  uint16_t rc = (catPage < totalPages-1) ? FLP_BLACK : FLP_DKGRAY;
  tft.fillTriangle(W-10, H/2, W-22, H/2-14, W-22, H/2+14, rc);

  // Page dots
  for (int p = 0; p < totalPages; p++) {
    int dotX = W/2 - totalPages*7 + p*14;
    if (p == catPage) tft.fillCircle(dotX, H-10, 4, FLP_BLACK);
    else              tft.drawCircle(dotX, H-10, 4, FLP_BLACK);
  }
}

// ═══════════════════════════════════════════════════════════════
// SUB MENU — scrollable list, 5 items visible
// ═══════════════════════════════════════════════════════════════
#define SUB_VISIBLE  5
#define SUB_ITEM_H  30

void drawSubMenu() {
  tft.fillScreen(FLP_ORANGE);
  drawStatusBar(categories[catSelected].label);
  drawTitleBar(categories[catSelected].label);

  int count = subCounts[catSelected];
  const char** subs = subMenus[catSelected];

  for (int i = 0; i < SUB_VISIBLE; i++) {
    int idx = subScroll + i;
    if (idx >= count) break;
    int iy  = CONTENT_Y + i * SUB_ITEM_H + 4;
    bool sel = (idx == subSelected);

    if (sel) {
      tft.fillRoundRect(8, iy, W-20, SUB_ITEM_H-4, 5, FLP_BLACK);
      tft.fillTriangle(W-14, iy+11, W-6, iy+19, W-14, iy+27, FLP_ORANGE);
      tft.setTextColor(FLP_ORANGE);
    } else {
      tft.fillRoundRect(8, iy, W-20, SUB_ITEM_H-4, 5, FLP_ORANGE);
      tft.drawRoundRect(8, iy, W-20, SUB_ITEM_H-4, 5, FLP_BLACK);
      tft.setTextColor(FLP_BLACK);
    }
    // Number badge
    tft.setTextSize(1);
    char num[4]; snprintf(num, sizeof(num), "%d", idx+1);
    tft.setCursor(16, iy+9); tft.print(num);
    // Label
    tft.setTextSize(2);
    tft.setCursor(36, iy+5); tft.print(subs[idx]);
  }

  // Scroll bar
  if (count > SUB_VISIBLE) {
    int barH   = CONTENT_H;
    int thumbH = max(16, barH * SUB_VISIBLE / count);
    int thumbY = CONTENT_Y + (subScroll * (barH - thumbH)) / max(1, count - SUB_VISIBLE);
    tft.fillRect(W-6, CONTENT_Y, 4, barH, FLP_DKGRAY);
    tft.fillRect(W-6, thumbY, 4, thumbH, FLP_BLACK);
  }

  drawNavBar("< BACK", "OK >");
}

// ═══════════════════════════════════════════════════════════════
// ACTION SCREEN
// ═══════════════════════════════════════════════════════════════
void drawActionScreen() {
  tft.fillScreen(FLP_ORANGE);
  drawStatusBar(subMenus[catSelected][subSelected]);
  drawTitleBar(subMenus[catSelected][subSelected]);

  tft.setTextColor(FLP_BLACK); tft.setTextSize(4);
  int iconW = strlen(categories[catSelected].icon) * 24;
  tft.setCursor((W-iconW)/2, CONTENT_Y+12);
  tft.print(categories[catSelected].icon);

  tft.setTextSize(2);
  int lw = 12 * 12;
  tft.setCursor((W-lw)/2, CONTENT_Y+62);
  tft.print("Coming Soon");

  tft.setTextSize(1);
  tft.setCursor((W-120)/2, CONTENT_Y+92);
  tft.print("Flash Bruce .bin for");
  tft.setCursor((W-120)/2, CONTENT_Y+106);
  tft.print("full functionality");

  drawNavBar("< BACK", nullptr);
}

// ═══════════════════════════════════════════════════════════════
// TOUCH HIT TESTING
// ═══════════════════════════════════════════════════════════════
int mainMenuTileTapped(int tx, int ty) {
  int startIdx = catPage * TILES_PER_PAGE;
  int endIdx   = min(startIdx + TILES_PER_PAGE, CAT_COUNT);
  int cols     = 2;
  int gridW    = cols * TILE_W + (cols-1) * TILE_GAP;
  int gridX    = (W - gridW) / 2;
  int gridY    = CONTENT_Y + 6;
  for (int i = 0; i < endIdx - startIdx; i++) {
    int col = i % cols, row = i / cols;
    int x0  = gridX + col * (TILE_W + TILE_GAP);
    int y0  = gridY + row * (TILE_H + TILE_GAP);
    if (tx >= x0 && tx < x0+TILE_W && ty >= y0 && ty < y0+TILE_H)
      return startIdx + i;
  }
  return -1;
}

int subMenuItemTapped(int tx, int ty) {
  if (tx < 70 && ty > H-20) return -2;  // back
  for (int i = 0; i < SUB_VISIBLE; i++) {
    int iy  = CONTENT_Y + i * SUB_ITEM_H + 4;
    int idx = subScroll + i;
    if (tx >= 8 && tx < W-20 && ty >= iy && ty < iy+SUB_ITEM_H-4)
      if (idx < subCounts[catSelected]) return idx;
  }
  return -1;
}

// ═══════════════════════════════════════════════════════════════
// SETUP & LOOP
// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(FLP_ORANGE);

  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  bootStart   = millis();
  lastFrameMs = millis();
  appState    = ST_BOOT;
  drawBoot(0);
}

void loop() {
  unsigned long now = millis();

  if (appState == ST_BOOT) {
    if (now - lastFrameMs > 140) {
      lastFrameMs = now;
      drawBoot(bootFrame++ % 8);
    }
    if (now - bootStart > 3000) {
      appState = ST_MAIN;
      drawMainMenu();
    }
    return;
  }

  Touch t = getTouch();
  if (!t.valid) return;

  if (appState == ST_MAIN) {
    int totalPages = (CAT_COUNT + TILES_PER_PAGE - 1) / TILES_PER_PAGE;

    // Left arrow
    if (t.x < 30 && abs(t.y - H/2) < 32 && catPage > 0) {
      catPage--; drawMainMenu(); return;
    }
    // Right arrow
    if (t.x > W-30 && abs(t.y - H/2) < 32 && catPage < totalPages-1) {
      catPage++; drawMainMenu(); return;
    }
    // Tile
    int tapped = mainMenuTileTapped(t.x, t.y);
    if (tapped >= 0) {
      if (tapped == catSelected) {
        subScroll = 0; subSelected = 0;
        appState  = ST_SUB; drawSubMenu();
      } else {
        catSelected = tapped; drawMainMenu();
      }
    }
    return;
  }

  if (appState == ST_SUB) {
    int item = subMenuItemTapped(t.x, t.y);
    if (item == -2) { appState = ST_MAIN; drawMainMenu(); return; }
    if (item >= 0) {
      if (item == subSelected) {
        appState = ST_ACTION; drawActionScreen();
      } else {
        subSelected = item;
        if (subSelected < subScroll) subScroll = subSelected;
        if (subSelected >= subScroll + SUB_VISIBLE) subScroll = subSelected - SUB_VISIBLE + 1;
        drawSubMenu();
      }
    }
    // Swipe scroll: top/bottom edge scroll triggers
    if (t.y < CONTENT_Y + 15 && subScroll > 0) { subScroll--; drawSubMenu(); }
    if (t.y > H-32 && t.y < H-16 && subScroll+SUB_VISIBLE < subCounts[catSelected]) { subScroll++; drawSubMenu(); }
    return;
  }

  if (appState == ST_ACTION) {
    if (t.x < 80 && t.y > H-20) { appState = ST_SUB; drawSubMenu(); }
    return;
  }
}
