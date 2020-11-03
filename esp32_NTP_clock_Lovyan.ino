#define LGFX_M5STACK          // M5 Stack boards
//#define LGFX_ESP_WROVER_KIT   // ESP-WROVER-KIT

#include <WiFi.h>
#include <LovyanGFX.hpp>

const char* WIFI_NAME     = "xxx";
const char* WIFI_PASS     = "xxx";

const char* TIME_ZONE       = "CET-1CEST,M3.5.0/2,M10.5.0/3";  //https://remotemonitoringsystems.ca/time-zone-abbreviations.php
const char* TIME_SERVER     = "nl.pool.ntp.org";

static LGFX lcd;
static LGFX_Sprite canvas(&lcd);       // オフスクリーン描画用バッファ
static LGFX_Sprite sevenSegment(&lcd);
static LGFX_Sprite clockbase(&canvas); // 時計の文字盤パーツ
static LGFX_Sprite needle1(&canvas);   // 長針・短針パーツ
static LGFX_Sprite shadow1(&canvas);   // 長針・短針の影パーツ
static LGFX_Sprite needle2(&canvas);   // 秒針パーツ
static LGFX_Sprite shadow2(&canvas);   // 秒針の影パーツ

static int32_t width = 239;             // 時計の縦横画像サイズ
static int32_t halfwidth = width >> 1;  // 時計盤の中心座標
static auto transpalette = 0;           // 透過色パレット番号
static float zoom;                      // 表示倍率

inline float mapFloat(const float &x, const float &in_min, const float &in_max, const float &out_min, const float &out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#if (defined(LGFX_M5STACK))

#define BUTTON_A_PIN    39
#define BUTTON_B_PIN    38
#define BUTTON_C_PIN    37
#define PRESS_TIME_MS   50
#define DEBOUNCE_MS     25

#include "Button.h"

Button BtnA = Button(BUTTON_A_PIN, true, DEBOUNCE_MS);
Button BtnB = Button(BUTTON_B_PIN, true, DEBOUNCE_MS);
Button BtnC = Button(BUTTON_C_PIN, true, DEBOUNCE_MS);

#endif

enum {DISCRETE, CONTINOUS, SEVEN_SEGMENT} clockMode{CONTINOUS};

void setup(void)
{
  lcd.init();
  lcd.setBrightness(20);
  lcd.setRotation(1);
  zoom = (float)(std::min(lcd.width(), lcd.height())) / width; // 表示が画面にフィットするよう倍率を調整

  lcd.setPivot(lcd.width() >> 1, lcd.height() >> 1); // 時計描画時の中心を画面中心に合わせる

  canvas.setColorDepth(lgfx::palette_4bit);  // 各部品を４ビットパレットモードで準備する
  clockbase.setColorDepth(lgfx::palette_4bit);
  needle1.setColorDepth(lgfx::palette_4bit);
  shadow1.setColorDepth(lgfx::palette_4bit);
  needle2.setColorDepth(lgfx::palette_4bit);
  shadow2.setColorDepth(lgfx::palette_4bit);

  sevenSegment.setColorDepth(lgfx::palette_4bit);
  sevenSegment.createSprite(lcd.width(), lcd.height());
  sevenSegment.setTextDatum(lgfx::middle_left);


  canvas.createSprite(width, width); // メモリ確保
  clockbase.createSprite(width, width);
  needle1.createSprite(9, 119);
  shadow1.createSprite(9, 119);
  needle2.createSprite(3, 119);
  shadow2.createSprite(3, 119);

  canvas.fillScreen(transpalette); // 透過色で背景を塗り潰す (create直後は0埋めされているので省略可能)
  clockbase.fillScreen(transpalette);
  needle1.fillScreen(transpalette);
  shadow1.fillScreen(transpalette);

  clockbase.setFont(&fonts::Font4);           // フォント種類を変更(時計盤の文字用)
  clockbase.setTextDatum(lgfx::middle_center);
  clockbase.fillCircle(halfwidth, halfwidth, halfwidth    ,  6); // 時計盤の背景の円を塗る
  clockbase.drawCircle(halfwidth, halfwidth, halfwidth - 1, 15);
  for (int i = 1; i <= 60; ++i) {
    float rad = i * 6 * - 0.0174532925;              // 時計盤外周の目盛り座標を求める
    float cosy = - cos(rad) * (halfwidth * 10 / 11);
    float sinx = - sin(rad) * (halfwidth * 10 / 11);
    bool flg = 0 == (i % 5);      // ５目盛り毎フラグ
    clockbase.fillCircle(halfwidth + sinx + 1, halfwidth + cosy + 1, flg * 3 + 1,  4); // 目盛りを描画
    clockbase.fillCircle(halfwidth + sinx    , halfwidth + cosy    , flg * 3 + 1, 12);
    if (flg) {                    // 文字描画
      cosy = - cos(rad) * (halfwidth * 10 / 13);
      sinx = - sin(rad) * (halfwidth * 10 / 13);
      clockbase.setTextColor(1);
      clockbase.drawNumber(i / 5, halfwidth + sinx + 1, halfwidth + cosy + 4);
      clockbase.setTextColor(15);
      clockbase.drawNumber(i / 5, halfwidth + sinx    , halfwidth + cosy + 3);
    }
  }
  clockbase.setFont(&fonts::Font7);

  needle1.setPivot(4, 100);  // 針パーツの回転中心座標を設定する
  shadow1.setPivot(4, 100);
  needle2.setPivot(1, 100);
  shadow2.setPivot(1, 100);

  for (int i = 6; i >= 0; --i) {  // 針パーツの画像を作成する
    needle1.fillTriangle(4, - 16 - (i << 1), 8, needle1.height() - (i << 1), 0, needle1.height() - (i << 1), 15 - i);
    shadow1.fillTriangle(4, - 16 - (i << 1), 8, shadow1.height() - (i << 1), 0, shadow1.height() - (i << 1),  1 + i);
  }
  for (int i = 0; i < 7; ++i) {
    needle1.fillTriangle(4, 16 + (i << 1), 8, needle1.height() + 32 + (i << 1), 0, needle1.height() + 32 + (i << 1), 15 - i);
    shadow1.fillTriangle(4, 16 + (i << 1), 8, shadow1.height() + 32 + (i << 1), 0, shadow1.height() + 32 + (i << 1),  1 + i);
  }
  needle1.fillTriangle(4, 32, 8, needle1.height() + 64, 0, needle1.height() + 64, 0);
  shadow1.fillTriangle(4, 32, 8, shadow1.height() + 64, 0, shadow1.height() + 64, 0);
  needle1.fillRect(0, 117, 9, 2, 15);
  shadow1.fillRect(0, 117, 9, 2,  1);
  needle1.drawFastHLine(1, 117, 7, 12);
  shadow1.drawFastHLine(1, 117, 7,  4);

  needle1.fillCircle(4, 100, 4, 15);
  shadow1.fillCircle(4, 100, 4,  1);
  needle1.drawCircle(4, 100, 4, 14);

  needle2.fillScreen(9);
  shadow2.fillScreen(3);
  needle2.drawFastVLine(1, 0, 119, 8);
  shadow2.drawFastVLine(1, 0, 119, 1);
  needle2.fillRect(0, 99, 3, 3, 8);

  lcd.startWrite();

  WiFi.begin(WIFI_NAME, WIFI_PASS);

  while (!WiFi.isConnected()) {
    delay(10);
  }

  configTzTime(TIME_ZONE, TIME_SERVER);

  struct tm timeinfo {
    0
  };

  while (!getLocalTime(&timeinfo, 0))
    vTaskDelay(10 / portTICK_PERIOD_MS);

  ESP_LOGI(TAG, "NTP sync from '%s'", TIME_SERVER);

#if (defined(LGFX_M5STACK))

  lcd.setTextDatum(top_center);
  lcd.setFont(&fonts::FreeSans12pt7b);
  lcd.drawString("esp32 NTP clock Lovyan", lcd.width() >> 1, 0);
  lcd.drawString("A = discrete", lcd.width() >> 1, 50);
  lcd.drawString("B = continous", lcd.width() >> 1, 100);
  lcd.drawString("C = lcd clock", lcd.width() >> 1, 150);
  lcd.drawString("press A to continue", lcd.width() >> 1, lcd.height() - lcd.fontHeight());

  while (!BtnA.wasReleasefor(PRESS_TIME_MS)) {
    BtnA.read();
    delay(2);
  }
  lcd.clear();

#endif
}

void draw7Seg(const time_t t)
{
  sevenSegment.setFont(&fonts::Font7);
  sevenSegment.setTextSize(2);
  sevenSegment.setCursor(20, sevenSegment.height() >> 1);
  sevenSegment.setTextDatum(middle_center);
  sevenSegment.setTextColor(3);  // 消去色で 88:88 を描画する
  sevenSegment.print("88:88");
  sevenSegment.setCursor(20, sevenSegment.height() >> 1);
  sevenSegment.setTextColor(12); // 表示色で時:分を描画する

  static struct tm timeinfo;
  localtime_r(&t, &timeinfo);
  sevenSegment.printf("%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

  //build a date string
  static char datestr[40];
  strftime(datestr, sizeof(datestr) - 1, "%a %B %d %G", &timeinfo);
  sevenSegment.setFont(&fonts::Font2);
  sevenSegment.setTextDatum(top_center);
  //delete previous 'datestr'
  static const int POS_Y {
    lcd.height() - sevenSegment.fontHeight()
  };
  sevenSegment.fillRect(0, POS_Y, lcd.width(), sevenSegment.fontHeight(), TFT_BLACK);
  sevenSegment.drawString(datestr, sevenSegment.width() >> 1 , POS_Y);

  //display the date
  sevenSegment.pushSprite(0, 0);
}

void updateMs(int32_t ms)
{ // 時計盤のデジタル表示部の描画
  int x = clockbase.getPivotX();
  int y = clockbase.getPivotY();
  clockbase.setTextSize(0);
  clockbase.setCursor(x, y);
  clockbase.setTextColor(5);  // 消去色で 88:88 を描画する
  clockbase.print("888");
  clockbase.setCursor(x, y);
  clockbase.setTextColor(12); // 表示色で時:分を描画する
  clockbase.printf("%03d", ms);
}

void drawClock()
{ // 時計の描画
  clockbase.pushSprite(0, 0);  // 描画用バッファに時計盤の画像を上書き

  static struct timeval microSecondTime;
  gettimeofday(&microSecondTime, NULL);
  static struct tm localTime;
  localtime_r(&microSecondTime.tv_sec, &localTime);

  float fhour = mapFloat(localTime.tm_hour, 0, 12.0, 0, 360.0);
  fhour += mapFloat(localTime.tm_min, 0, 60.0, 0, 30.0);

  float fmin = mapFloat(localTime.tm_min, 0, 60.0, 0, 360.0);
  if (clockMode == CONTINOUS)
    fmin += mapFloat(localTime.tm_sec, 0, 60.0, 0, 6.0);

  float fsec = mapFloat(localTime.tm_sec, 0, 60.0, 0, 360.0);
  if (clockMode == CONTINOUS)
    fsec += mapFloat(microSecondTime.tv_usec, 0, 1000 * 1000.0, 0, 6.0);

  int px = canvas.getPivotX();
  int py = canvas.getPivotY();
  shadow1.pushRotateZoom(px + 2, py + 2, fhour, 1.0, 0.7, transpalette); // 針の影を右下方向にずらして描画する
  shadow1.pushRotateZoom(px + 3, py + 3, fmin , 1.0, 1.0, transpalette);
  shadow2.pushRotateZoom(px + 4, py + 4, fsec , 1.0, 1.0, transpalette);
  needle1.pushRotateZoom(          fhour, 1.0, 0.7, transpalette); // 針を描画する
  needle1.pushRotateZoom(          fmin , 1.0, 1.0, transpalette);
  needle2.pushRotateZoom(          fsec , 1.0, 1.0, transpalette);

  canvas.pushRotateZoom(0, zoom, zoom, transpalette);    // 完了した時計盤をLCDに描画する
}

void loop(void)
{
#if (defined(LGFX_M5STACK))

  BtnA.read();
  if (BtnA.wasReleasefor(PRESS_TIME_MS)) {
    if (clockMode == SEVEN_SEGMENT) lcd.clear();
    clockMode = DISCRETE;
  }

  BtnB.read();
  if (BtnB.wasReleasefor(PRESS_TIME_MS)) {
    if (clockMode == SEVEN_SEGMENT) lcd.clear();
    clockMode = CONTINOUS;
  }

  BtnC.read();
  if (BtnC.wasReleasefor(PRESS_TIME_MS)) {
    clockMode = SEVEN_SEGMENT;
  }

#endif

  switch (clockMode) {
    case DISCRETE :
      {
        static time_t prev;
        time_t now = time(NULL);
        if (now != prev) {
          drawClock();
          prev = now;
        }
      }
      break;

    case CONTINOUS :
      drawClock();
      break;

    case SEVEN_SEGMENT :
      {
        static time_t prev;
        time_t now = time(NULL);
        if (now != prev) {
          draw7Seg(now);
          prev = now;
        }
      }
      break;
    default : ESP_LOGE(TAG, "Unhandled clockMode");
  }
}
