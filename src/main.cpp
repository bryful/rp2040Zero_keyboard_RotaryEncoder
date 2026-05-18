/*
4x5 キーマトリクス
Rows（出力）
Row	GPIO
R0	GP5
R1	GP4
R2	GP3
R3	GP2


Cols（入力_PULLUP）
Col	GPIO
C0	GP6
C1	GP7
C2	GP8
C3	GP9
C4	GP10

┌────┬────┐────┐────┐
│ESC │Save│    │    │
├────┼────┼────┼────┤
│Undo│Redo│Copy│Paste│
├────┼────┼────┼────┤
│Zoom-│Zoom+│Cut │Enter│
├────┼────┼────┼────┤
│ [  │  ] │Hand│Eye │
├────┼────┼────┼────┤
│Shift│Ctrl│Alt │Space│
└────┴────┴────┴────┘

// 固定キー
┌─────┬─────┬─────┐─────┐─────┐
│ ESC │ Copy│ Cut │Paste│  %1 │ %1はModeチェンジ。4Modeを順に切り替える。LEDの色を変える
├─────┼─────┼─────┼─────┼─────┤
│ Save│     │     │     │     │
├─────│─────┼─────┼─────┼─────┤
│ Redo│     │     │     │  MR │ MR Mouse Right//Click
├─────│─────┼─────┼─────┼─────┤
│ Undo│Shift│ Ctrl│ Alt │Space│
└─────┴─────┴─────┴─────┴─────┘
// PhotoShopのショートカット
┌─────┬─────┬─────┐─────┐─────┐
│ ESC │ Copy│ Cut │Paste│  %1 │
├─────┼─────┼─────┼─────┼─────┤
│  R  │Ctl+D│Ctl+A│S+C+I│Alt+Del│
├─────│─────┼─────┼─────┼─────┤
│ Redo│  M  │  V  │  H  │  MR │
├─────│─────┼─────┼─────┼─────┤
│ Undo│Shift│ Ctrl│ Alt │Space│
└─────┴─────┴─────┴─────┴─────┘
RE1 Ctrl+k+ Ctrl+k- Ctrl+0
RE2 [ ] B
// AfterEffectsのショートカット
┌─────┬─────┬─────┐─────┐─────┐
│ ESC │ Copy│ Cut │Paste│  %1 │
├─────┼─────┼─────┼─────┼─────┤
│  *  │Ctr+0│C+S+D│Ctr+D│  0  │
├─────│─────┼─────┼─────┼─────┤
│ Redo│C+A+0│  V  │  H  │  MR │
├─────│─────┼─────┼─────┼─────┤
│ Undo│Shift│ Ctrl│ Alt │Space│
└─────┴─────┴─────┴─────┴─────┘
ビューzo,,, zi....
タイムラインzo--- zi^^^
// Illustratorのショートカット
┌─────┬─────┬─────┐─────┐─────┐
│ ESC │ Copy│ Cut │Paste│  %1 │
├─────┼─────┼─────┼─────┼─────┤
│  I  │  P  │ S+C │ S++ │  -  │
├─────│─────┼─────┼─────┼─────┤
│ Redo│  A  │  V  │  H  │  MR │
├─────│─────┼─────┼─────┼─────┤
│ Undo│Shift│ Ctrl│ Alt │Space│
└─────┴─────┴─────┴─────┴─────┘
RE2 Shift+H V Shift+Ctrl+1
RE1 Ctrl+k+ Ctrl+k- Ctrl+0



ロータリーエンコーダ1
Signal	GPIO
CLK	GP11
DT	GP12
SW	GP13

ロータリーエンコーダ2
Signal	GPIO
CLK	GP14
DT	GP15
SW	GP26
*/
#include <Arduino.h>
#include <Keyboard.h>
#include <Mouse.h>
#include <CoreMutex.h>
#include <RP2040USB.h>
#include <tusb.h>
#include "class/hid/hid.h"
#include "class/hid/hid_device.h"
#include <Adafruit_NeoPixel.h>
#include "USBHIDKeyboard_JIS.h"

// ============================================
// WS2812
// ============================================
#define LED_PIN 16
#define LED_COUNT 1
Adafruit_NeoPixel led(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ============================================
// スクリーンセーバー対策
// ============================================
constexpr uint32_t SCREENSAVER_TIMEOUT_MS = 60000 * 3; // 3分
uint32_t lastInputTime = 0;

// ============================================
// 色定義
// ============================================
uint32_t COLOR_OFF;
uint32_t COLOR_RED;
uint32_t COLOR_GREEN;
uint32_t COLOR_BLUE;
uint32_t COLOR_YELLOW;
uint32_t COLOR_MAGENTA;
uint32_t COLOR_CYAN;
uint32_t COLOR_DARK_RED;
uint32_t COLOR_DARK_GREEN;
uint32_t COLOR_DARK_BLUE;
uint32_t COLOR_DARK_YELLOW;
uint32_t COLOR_DARK_MAGENTA;
uint32_t COLOR_DARK_CYAN;
uint32_t COLOR_WHITE;
uint32_t COLOR_GRAY;
// ============================================
// 型定義
// ============================================
const uint8_t rowPins[4] = {5, 4, 3, 2};
const uint8_t colPins[5] = {6, 7, 8, 9, 10};

enum ClickType
{
  NONE = 0,
  MOUSE_L = MOUSE_LEFT,
  MOUSE_R = MOUSE_RIGHT,
  MOUSE_M = MOUSE_MIDDLE
};

struct KeyConfig
{
  uint8_t modifier;
  uint8_t keycode;
  ClickType mouse;
};

// エンコーダの動的状態のみを管理する構造体
struct RotaryEncoder
{
  uint8_t pinA;
  uint8_t pinB;
  uint8_t pinSW;
  int lastStateA;
  bool swOldState;

  // キュー処理用メンバ
  int pulseQueue;       // 溜まったパルス数（正:右, 負:左）
  uint32_t activeTimer; // キーを押し下げているタイマー
  bool isKeyActive;     // 現在エンコーダ由来のキーがONか
  KeyConfig activeCfg;  // 現在ONにしているキー設定
};

// モード切替用の特殊キー定義 (%1として使用)
#define KEY_MODE_CHANGE 0xFF

// ============================================
// モード・キーマップ管理
// ============================================
enum ActiveMode
{
  MODE_PHOTOSHOP = 0,
  MODE_AFTEREFFECTS,
  MODE_ILLUSTRATOR,
  NUM_MODES
};
uint8_t currentMode = MODE_PHOTOSHOP;

#define ENCODER_COUNT 2

KeyConfig keyMaps[NUM_MODES][4][5] = {
    {{{0, HID_KEY_ESCAPE, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_C, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_X, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_V, NONE}, {0, KEY_MODE_CHANGE, NONE}},
     {{0, HID_KEY_R, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_D, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_A, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_I, NONE}, {KEYBOARD_MODIFIER_LEFTALT, HID_KEY_DELETE, NONE}},
     {{KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_Z, NONE}, {0, HID_KEY_M, NONE}, {0, HID_KEY_V, NONE}, {0, HID_KEY_H, NONE}, {0, HID_KEY_NONE, MOUSE_R}},
     {{KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_Z, NONE}, {KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_NONE, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_NONE, NONE}, {KEYBOARD_MODIFIER_LEFTALT, HID_KEY_NONE, NONE}, {0, HID_KEY_SPACE, NONE}}},
    {{{0, HID_KEY_ESCAPE, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_C, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_X, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_V, NONE}, {0, KEY_MODE_CHANGE, NONE}},
     {{0, HID_KEY_KP_ASTERISK, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_0, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_D, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_D, NONE}, {0, HID_KEY_0, NONE}},
     {{KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_Z, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTALT, HID_KEY_0, NONE}, {0, HID_KEY_V, NONE}, {0, HID_KEY_H, NONE}, {0, HID_KEY_NONE, MOUSE_R}},
     {{KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_Z, NONE}, {KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_NONE, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_NONE, NONE}, {KEYBOARD_MODIFIER_LEFTALT, HID_KEY_NONE, NONE}, {0, HID_KEY_SPACE, NONE}}},
    {{{0, HID_KEY_ESCAPE, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_C, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_X, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_V, NONE}, {0, KEY_MODE_CHANGE, NONE}},
     {{0, HID_KEY_I, NONE}, {0, HID_KEY_P, NONE}, {KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_C, NONE}, {KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_KP_PLUS, NONE}, {0, HID_KEY_MINUS, NONE}},
     {{KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_Z, NONE}, {0, HID_KEY_A, NONE}, {0, HID_KEY_V, NONE}, {0, HID_KEY_H, NONE}, {0, HID_KEY_NONE, MOUSE_R}},
     {{KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_Z, NONE}, {KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_NONE, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_NONE, NONE}, {KEYBOARD_MODIFIER_LEFTALT, HID_KEY_NONE, NONE}, {0, HID_KEY_SPACE, NONE}}}};

KeyConfig encoderMaps[NUM_MODES][ENCODER_COUNT][3] = {
    {// Enc0
     {{KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_KP_PLUS, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_KP_MINUS, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_0, NONE}},
     // Enc1
     {{0, HID_KEY_LEFTBRACE, NONE}, {0, HID_KEY_RIGHTBRACE, NONE}, {0, HID_KEY_B, NONE}}},
    {// Enc0
     {{0, HID_KEY_MINUS, NONE}, {0, HID_KEY_CARET, NONE}, {0, HID_KEY_NONE, NONE}},
     // Enc1
     {{0, HID_KEY_COMMA, NONE}, {0, HID_KEY_DOT, NONE}, {0, HID_KEY_NONE, NONE}}},
    {// Enc0
     {{KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_KP_PLUS, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_KP_MINUS, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_0, NONE}},
     // Enc1
     {{KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_H, NONE}, {0, HID_KEY_V, NONE}, {KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_1, NONE}}}};

// constexpr int ENCODER_COUNT = sizeof(encoders) / sizeof(encoders[0]);

// エンコーダーのハードウェア接続設定
RotaryEncoder encoders[] = {
    {11, 12, 13, LOW, true, 0, 0}, // エンコーダ1 (Enc0)
    {14, 15, 26, LOW, true, 0, 0}  // エンコーダ2 (Enc1)};
};
bool keyState[4][5] = {false};
// ============================================
// 【新規】同時押し用 一括送信レポートバッファ
// ============================================
uint8_t currentReportModifier = 0;
uint8_t currentReportKeys[6] = {0};

// マウスボタン個別追跡用フラグ
bool currentMouseL = false;
bool currentMouseR = false;
bool currentMouseM = false;

// バッファに通常キーを安全に追加する関数（最大6個まで）
void addKeyToReport(uint8_t keycode)
{
  if (keycode == HID_KEY_NONE)
    return;
  for (int i = 0; i < 6; i++)
  {
    if (currentReportKeys[i] == keycode)
      return;
    if (currentReportKeys[i] == 0)
    {
      currentReportKeys[i] = keycode;
      return;
    }
  }
}
void addMouseToReport(ClickType mouseButton)
{
  if (mouseButton == MOUSE_L)
    currentMouseL = true;
  if (mouseButton == MOUSE_R)
    currentMouseR = true;
  if (mouseButton == MOUSE_M)
    currentMouseM = true;
}
// ============================================
// LED制御関数
// ============================================
void setLed(uint32_t color)
{
  led.setPixelColor(0, color);
  led.show();
}

void updateModeLED()
{
  switch (currentMode)
  {
  case MODE_PHOTOSHOP:
    setLed(COLOR_OFF);
    break; // Photoshop: 青
  case MODE_AFTEREFFECTS:
    setLed(COLOR_DARK_BLUE);
    break; // AfterEffects: マゼンタ
  case MODE_ILLUSTRATOR:
    setLed(COLOR_DARK_YELLOW);
    break; // Illustrator: 黄
  }
}
// ============================================
// HID送信コア関数（一括レポート送信仕様）
// ============================================
void sendReportFinal()
{
  CoreMutex m(&__usb_mutex);
  tud_task();
  if (!__USBHIDReady())
    return;

  tud_hid_keyboard_report(__USBGetKeyboardReportID(), currentReportModifier, currentReportKeys);

  if (currentMouseL)
    Mouse.press(MOUSE_LEFT);
  else
    Mouse.release(MOUSE_LEFT);
  if (currentMouseR)
    Mouse.press(MOUSE_RIGHT);
  else
    Mouse.release(MOUSE_RIGHT);
  if (currentMouseM)
    Mouse.press(MOUSE_MIDDLE);
  else
    Mouse.release(MOUSE_MIDDLE);

  tud_task();
}
// ============================================
// HID送信コア関数
// ============================================
/*
void sendHIDReport(uint8_t modifier, uint8_t keycode, ClickType mouseButton, bool isPressed)
{
  CoreMutex m(&__usb_mutex);
  tud_task();
  if (!__USBHIDReady())
    return;

  uint8_t keys[6] = {0};
  if (isPressed)
  {
    keys[0] = keycode;
  }
  tud_hid_keyboard_report(__USBGetKeyboardReportID(), isPressed ? modifier : 0, keys);

  if (mouseButton != NONE)
  {
    if (isPressed)
    {
      Mouse.press(mouseButton);
    }
    else
    {
      Mouse.release(mouseButton);
    }
  }

  tud_task();
  if (isPressed)
  {
    lastInputTime = millis();
  }
}

void tapKey(KeyConfig cfg)
{
  sendHIDReport(cfg.modifier, cfg.keycode, cfg.mouse, true);
  delay(10);
  sendHIDReport(cfg.modifier, cfg.keycode, cfg.mouse, false);
}
*/
// ============================================
// 監視ロジック
// ============================================
void updateEncoders()
{
  for (int i = 0; i < ENCODER_COUNT; i++)
  {
    auto &enc = encoders[i];

    // --- 1. 回転のエッジ検出 (高速回転でもキューに溜めて絶対に取りこぼさない) ---
    int currentStateA = digitalRead(enc.pinA);
    if (currentStateA != enc.lastStateA && currentStateA == LOW)
    {
      if (digitalRead(enc.pinB) != currentStateA)
      {
        enc.pulseQueue++;
      } // 右回転をストック
      else
      {
        enc.pulseQueue--;
      } // 左回転をストック
    }
    enc.lastStateA = currentStateA;

    // --- 2. ストックされたパルスの処理（キーのON/OFF管理） ---
    if (enc.isKeyActive)
    {
      // キーの押し下げ時間が20msを超えたら一旦離す（OSが認識するのに最適な時間）
      if (millis() - enc.activeTimer >= 20)
      {
        enc.isKeyActive = false;
      }
      else
      {
        // 20ms経過するまでは、現在のキー状態を維持してバッファに載せ続ける
        currentReportModifier |= enc.activeCfg.modifier;
        addKeyToReport(enc.activeCfg.keycode);
        if (enc.activeCfg.mouse != NONE)
          addMouseToReport(enc.activeCfg.mouse);
      }
    }
    else
    {
      // キューに未処理の回転が溜まっていて、かつ前回のキーが解除されていれば次のパルスを処理
      if (enc.pulseQueue != 0)
      {
        if (enc.pulseQueue > 0)
        {
          enc.activeCfg = encoderMaps[currentMode][i][0]; // CW
          enc.pulseQueue--;
        }
        else
        {
          enc.activeCfg = encoderMaps[currentMode][i][1]; // CCW
          enc.pulseQueue++;
        }

        enc.isKeyActive = true;
        enc.activeTimer = millis();
        lastInputTime = millis();

        // 最初の1フレーム目としてバッファに載せる
        currentReportModifier |= enc.activeCfg.modifier;
        addKeyToReport(enc.activeCfg.keycode);
        if (enc.activeCfg.mouse != NONE)
          addMouseToReport(enc.activeCfg.mouse);
      }
    }

    // --- 3. スイッチ（押し込み）検出 ---
    bool swState = digitalRead(enc.pinSW);
    if (swState == LOW)
    {
      KeyConfig cfg = encoderMaps[currentMode][i][2];
      currentReportModifier |= cfg.modifier;
      addKeyToReport(cfg.keycode);
      if (cfg.mouse != NONE)
        addMouseToReport(cfg.mouse);
      lastInputTime = millis();
    }
    enc.swOldState = swState;
  }
}

void updateKeyMatrix()
{
  for (int r = 0; r < 4; r++)
  {
    pinMode(rowPins[r], OUTPUT);
    digitalWrite(rowPins[r], LOW);

    for (int c = 0; c < 5; c++)
    {
      bool isPressed = (digitalRead(colPins[c]) == LOW);

      if (isPressed)
      {
        KeyConfig cfg = keyMaps[currentMode][r][c];

        if (cfg.keycode == KEY_MODE_CHANGE)
        {
          if (keyState[r][c] == false)
          {
            currentMode = (currentMode + 1) % NUM_MODES;
            updateModeLED();
          }
        }
        else
        {
          currentReportModifier |= cfg.modifier;
          addKeyToReport(cfg.keycode);
          if (cfg.mouse != NONE)
            addMouseToReport(cfg.mouse);
          lastInputTime = millis();
        }
      }
      keyState[r][c] = isPressed;
    }
    digitalWrite(rowPins[r], HIGH);
    pinMode(rowPins[r], INPUT_PULLUP);
  }
}

void sendMouseJiggle()
{
  Mouse.move(10, 0, 0);
  delay(10);
  Mouse.move(-10, 0, 0);
}

void handleScreensaver()
{
  if ((millis() - lastInputTime) >= SCREENSAVER_TIMEOUT_MS)
  {
    uint32_t prevColor = led.getPixelColor(0);
    setLed(COLOR_OFF);
    delay(100);
    setLed(COLOR_WHITE);
    delay(100);
    setLed(COLOR_OFF);
    delay(100);
    setLed(prevColor);
    sendMouseJiggle();
    lastInputTime = millis();
  }
}

// ============================================
// Setup / Loop
// ============================================
void setup()
{
  led.begin();

  COLOR_OFF = led.Color(0, 0, 0);
  COLOR_RED = led.Color(255, 0, 0);
  COLOR_GREEN = led.Color(0, 255, 0);
  COLOR_BLUE = led.Color(0, 0, 255);
  COLOR_YELLOW = led.Color(255, 255, 0);
  COLOR_MAGENTA = led.Color(255, 0, 255);
  COLOR_CYAN = led.Color(0, 255, 255);
  COLOR_DARK_RED = led.Color(64, 0, 0);
  COLOR_DARK_GREEN = led.Color(0, 64, 0);
  COLOR_DARK_BLUE = led.Color(0, 0, 64);
  COLOR_DARK_YELLOW = led.Color(64, 64, 0);
  COLOR_DARK_MAGENTA = led.Color(64, 0, 64);
  COLOR_DARK_CYAN = led.Color(0, 64, 64);
  COLOR_CYAN = led.Color(0, 255, 255);
  COLOR_WHITE = led.Color(255, 255, 255);
  COLOR_GRAY = led.Color(64, 64, 64);
  setLed(COLOR_RED);
  Serial.begin(115200);

  for (auto &enc : encoders)
  {
    pinMode(enc.pinA, INPUT_PULLUP);
    pinMode(enc.pinB, INPUT_PULLUP);
    pinMode(enc.pinSW, INPUT_PULLUP);
  }

  for (uint8_t p : rowPins)
    pinMode(p, INPUT_PULLUP);
  for (uint8_t p : colPins)
    pinMode(p, INPUT_PULLUP);

  Keyboard.begin();
  Mouse.begin();

  for (int i = 0; i < 4; i++)
  {
    setLed(COLOR_GREEN);
    delay(150);
    setLed(COLOR_OFF);
    delay(150);
  }
  updateModeLED();
  Serial.println("Keyboard Ready");
}

void loop()
{
  // 1. 毎フレームレポートバッファをリセット
  currentReportModifier = 0;
  memset(currentReportKeys, 0, sizeof(currentReportKeys));
  currentMouseL = false;
  currentMouseR = false;
  currentMouseM = false;

  // 2. 現在の物理的な「押し下げ状態」をスキャンして蓄積
  updateEncoders();
  updateKeyMatrix();

  // 3. 確定した最新レポートを一括でPCへ送信
  sendReportFinal();

  handleScreensaver();
  delay(1);
}