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
#include <LittleFS.h> // LittleFSを追加

/*
4x5 キーマトリクス (Row出力 / Col入力 反転対応版)
Rows（出力） -> GP5, GP4, GP3, GP2
Cols（入力_PULLUP） -> GP6, GP7, GP8, GP9, GP10

ロータリーエンコーダ1 (Enc0)
CLK: GP11, DT: GP12, SW: GP13

ロータリーエンコーダ2 (Enc1)
CLK: GP14, DT: GP15, SW: GP26
*/

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

// ============================================
// 型定義・構造体
// ============================================
// キーマトリクスのピン配置 誤配線があったため、行と列のピン番号を入れ替え
const uint8_t rowPins[4] = {5, 4, 3, 2};
// const uint8_t rowPins[4] = {2, 3, 4, 5};
const uint8_t colPins[5] = {6, 7, 8, 9, 10};

enum ClickType
{
  NONE = 0,
  MOUSE_L = MOUSE_LEFT,
  MOUSE_R = MOUSE_RIGHT,
  MOUSE_M = MOUSE_MIDDLE
};

struct __attribute__((packed)) KeyConfig
{
  uint8_t modifier;
  uint8_t keycode;
  ClickType mouse;
};

struct RotaryEncoder
{
  uint8_t pinA;
  uint8_t pinB;
  uint8_t pinSW;
  int lastStateA;
  bool swOldState;

  int pulseQueue;
  uint32_t activeTimer;
  bool isKeyActive;
  KeyConfig activeCfg;
};

// モード切替用の特殊キー定義 (%1として使用)
#define KEY_MODE_CHANGE 0xFF
#define ENCODER_COUNT 2
#define MAX_LAYERS 8 // 最大レイヤー数を8に設定（メモリ空間に余裕あり）
#define CONFIG_FILE_PATH "/keymap.bin"

// 【新規仕様】1レイヤーの情報を内包する構造体
struct __attribute__((packed)) LayerInfo
{
  char layerName[16]; // レイヤー名（PCアプリ連動用）
  uint8_t ledR;       // モードLEDのRGB値
  uint8_t ledG;
  uint8_t ledB;
  KeyConfig matrix[4][5];               // 4x5キーマトリクス
  KeyConfig encoders[ENCODER_COUNT][3]; // エンコーダー2個の設定 [0:CW, 1:CCW, 2:SW]
};

// ============================================
// モード・キーマップ管理（動的可変対応）
// ============================================
uint8_t numActiveModes = 3; // 現在の有効レイヤー数（LittleFSから動的ロード）
uint8_t currentMode = 0;    // インデックス管理（0スタート）

// 最大数分でレイヤー配列をメモリ上に静的確保
LayerInfo layers[MAX_LAYERS];

RotaryEncoder encoders[] = {
    {11, 12, 13, LOW, true, 0, 0, false, {0, 0, NONE}},
    {14, 15, 26, LOW, true, 0, 0, false, {0, 0, NONE}}};
bool keyState[4][5] = {false};

// ============================================
// 同時押し用 一括送信レポートバッファ
// ============================================
uint8_t currentReportModifier = 0;
uint8_t currentReportKeys[6] = {0};

bool currentMouseL = false;
bool currentMouseR = false;
bool currentMouseM = false;

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
void blinkLed(uint32_t color, int times, int delayMs)
{
  for (int i = 0; i < times; i++)
  {
    setLed(color);
    delay(delayMs);
    setLed(COLOR_OFF);
    delay(delayMs);
  }
}
// 【仕様変更】LayerInfoのRGB値からLEDを動的更新
void updateModeLED()
{
  if (currentMode < numActiveModes)
  {
    setLed(led.Color(layers[currentMode].ledR, layers[currentMode].ledG, layers[currentMode].ledB));
  }
  else
  {
    setLed(COLOR_OFF);
  }
}

// ============================================
// LittleFS 読み書き・シリアル通信処理
// ============================================
void loadConfigFromFlash()
{
  if (!LittleFS.begin())
    return;
  if (LittleFS.exists(CONFIG_FILE_PATH))
  {
    File f = LittleFS.open(CONFIG_FILE_PATH, "r");
    if (f)
    {
      f.read(&numActiveModes, 1);                // 1バイト目に有効レイヤー数
      f.read((uint8_t *)layers, sizeof(layers)); // 続いて構造体配列全体をロード
      f.close();
    }
  }
}

void handleSerialCommunication()
{
  if (Serial.available() > 0)
  {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    // PCからの保存要求受付
    if (cmd == "SAVE_CONFIG")
    {
      Serial.println("READY"); // 準備完了の返事

      // 有効レイヤー数(1バイト)を受信
      while (Serial.available() < 1)
      {
        tud_task();
      }
      Serial.readBytes(&numActiveModes, 1);
      if (numActiveModes > MAX_LAYERS)
        numActiveModes = MAX_LAYERS;

      // LayerInfo構造体配列の生バイナリを受信
      uint8_t *buf = (uint8_t *)layers;
      size_t totalSize = sizeof(layers);
      size_t received = 0;
      while (received < totalSize)
      {
        tud_task();
        if (Serial.available() > 0)
        {
          size_t amt = Serial.readBytes(buf + received, totalSize - received);
          received += amt;
        }
      }

      // LittleFSに即時書き込み
      File f = LittleFS.open(CONFIG_FILE_PATH, "w");
      if (f)
      {
        f.write(&numActiveModes, 1);
        f.write((uint8_t *)layers, sizeof(layers));
        f.close();
        Serial.println("SUCCESS");
        blinkLed(led.Color(0, 255, 0), 3, 200);
      }
      else
      {
        Serial.println("ERROR_FILE_OPEN");
        blinkLed(led.Color(255, 0, 0), 3, 200);
      }
      currentMode = 0; // モードを先頭にリセット
      updateModeLED();
    }
    // PCからの現在の設定吸い出し要求受付
    else if (cmd == "LOAD_CONFIG")
    {
      // 「有効レイヤー数」＋「バイナリ全体」を一括でPCへエコーバック
      Serial.write(&numActiveModes, 1);
      Serial.write((uint8_t *)layers, sizeof(layers));
      Serial.flush();
      blinkLed(led.Color(0, 0, 255), 3, 200);
      updateModeLED();
    }
  }
}

// ============================================
// HID送信コア関数
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
// 監視ロジック
// ============================================
void updateEncoders()
{
  for (int i = 0; i < ENCODER_COUNT; i++)
  {
    auto &enc = encoders[i];

    // --- 1. 回転のエッジ検出 ---
    int currentStateA = digitalRead(enc.pinA);
    if (currentStateA != enc.lastStateA && currentStateA == LOW)
    {
      if (digitalRead(enc.pinB) != currentStateA)
      {
        enc.pulseQueue++;
      }
      else
      {
        enc.pulseQueue--;
      }
    }
    enc.lastStateA = currentStateA;

    // --- 2. パルス処理 ---
    if (enc.isKeyActive)
    {
      if (millis() - enc.activeTimer >= 20)
      {
        enc.isKeyActive = false;
      }
      else
      {
        currentReportModifier |= enc.activeCfg.modifier;
        addKeyToReport(enc.activeCfg.keycode);
        if (enc.activeCfg.mouse != NONE)
          addMouseToReport(enc.activeCfg.mouse);
      }
    }
    else
    {
      if (enc.pulseQueue != 0)
      {
        if (enc.pulseQueue > 0)
        {
          enc.activeCfg = layers[currentMode].encoders[i][0]; // CW (layers経由に変更)
          enc.pulseQueue--;
        }
        else
        {
          enc.activeCfg = layers[currentMode].encoders[i][1]; // CCW
          enc.pulseQueue++;
        }

        enc.isKeyActive = true;
        enc.activeTimer = millis();
        lastInputTime = millis();

        currentReportModifier |= enc.activeCfg.modifier;
        addKeyToReport(enc.activeCfg.keycode);
        if (enc.activeCfg.mouse != NONE)
          addMouseToReport(enc.activeCfg.mouse);
      }
    }

    // --- 3. スイッチ検出 ---
    bool swState = digitalRead(enc.pinSW);
    if (swState == LOW)
    {
      KeyConfig cfg = layers[currentMode].encoders[i][2]; // SW
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
        KeyConfig cfg = layers[currentMode].matrix[r][c]; // layers経由に変更

        if (cfg.keycode == KEY_MODE_CHANGE)
        {
          if (keyState[r][c] == false)
          {
            // 動的な有効レイヤー数（numActiveModes）でループ処理
            currentMode = (currentMode + 1) % numActiveModes;
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
    setLed(led.Color(0, 0, 0));
    delay(100);
    setLed(led.Color(255, 255, 255));
    delay(100);
    setLed(led.Color(0, 0, 0));
    delay(100);
    setLed(prevColor);
    sendMouseJiggle();
    lastInputTime = millis();
  }
}

// 元のハードコーディングされていたデフォルト設定を構造体に初期展開する関数
void initDefaultKeymaps()
{
  numActiveModes = 3;
  layers[0].ledR = 0;
  layers[0].ledG = 0;
  layers[0].ledB = 0; // 黒(消灯)
  // Photoshop レイヤー設定 (Layer 0)
  strcpy(layers[0].layerName, "Photoshop");
  layers[0].matrix[0][0] = {0, HID_KEY_ESCAPE, NONE};
  layers[0].matrix[0][1] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_C, NONE};
  layers[0].matrix[0][2] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_X, NONE};
  layers[0].matrix[0][3] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_V, NONE};
  layers[0].matrix[0][4] = {0, KEY_MODE_CHANGE, NONE};
  layers[0].matrix[1][0] = {0, HID_KEY_R, NONE};
  layers[0].matrix[1][1] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_D, NONE};
  layers[0].matrix[1][2] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_A, NONE};
  layers[0].matrix[1][3] = {KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_I, NONE};
  layers[0].matrix[1][4] = {KEYBOARD_MODIFIER_LEFTALT, HID_KEY_DELETE, NONE};
  layers[0].matrix[2][0] = {KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_Z, NONE};
  layers[0].matrix[2][1] = {0, HID_KEY_M, NONE};
  layers[0].matrix[2][2] = {0, HID_KEY_V, NONE};
  layers[0].matrix[2][3] = {0, HID_KEY_H, NONE};
  layers[0].matrix[2][4] = {0, HID_KEY_NONE, MOUSE_R};
  layers[0].matrix[3][0] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_Z, NONE};
  layers[0].matrix[3][1] = {KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_NONE, NONE};
  layers[0].matrix[3][2] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_NONE, NONE};
  layers[0].matrix[3][3] = {KEYBOARD_MODIFIER_LEFTALT, HID_KEY_NONE, NONE};
  layers[0].matrix[3][4] = {0, HID_KEY_SPACE, NONE};
  layers[0].encoders[0][0] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_KP_PLUS, NONE};
  layers[0].encoders[0][1] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_KP_MINUS, NONE};
  layers[0].encoders[0][2] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_0, NONE};
  layers[0].encoders[1][0] = {0, HID_KEY_LEFTBRACE, NONE};
  layers[0].encoders[1][1] = {0, HID_KEY_RIGHTBRACE, NONE};
  layers[0].encoders[1][2] = {0, HID_KEY_B, NONE};

  // AfterEffects レイヤー設定 (Layer 1)
  strcpy(layers[1].layerName, "AfterEffects");
  layers[1].ledR = 0;
  layers[1].ledG = 0;
  layers[1].ledB = 64; // 暗い青
  layers[1].matrix[0][0] = {0, HID_KEY_ESCAPE, NONE};
  layers[1].matrix[0][1] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_C, NONE};
  layers[1].matrix[0][2] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_X, NONE};
  layers[1].matrix[0][3] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_V, NONE};
  layers[1].matrix[0][4] = {0, KEY_MODE_CHANGE, NONE};
  layers[1].matrix[1][0] = {0, HID_KEY_KP_ASTERISK, NONE};
  layers[1].matrix[1][1] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_0, NONE};
  layers[1].matrix[1][2] = {KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_D, NONE};
  layers[1].matrix[1][3] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_D, NONE};
  layers[1].matrix[1][4] = {0, HID_KEY_0, NONE};
  layers[1].matrix[2][0] = {KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_Z, NONE};
  layers[1].matrix[2][1] = {KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTALT, HID_KEY_0, NONE};
  layers[1].matrix[2][2] = {0, HID_KEY_V, NONE};
  layers[1].matrix[2][3] = {0, HID_KEY_H, NONE};
  layers[1].matrix[2][4] = {0, HID_KEY_NONE, MOUSE_R};
  layers[1].matrix[3][0] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_Z, NONE};
  layers[1].matrix[3][1] = {KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_NONE, NONE};
  layers[1].matrix[3][2] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_NONE, NONE};
  layers[1].matrix[3][3] = {KEYBOARD_MODIFIER_LEFTALT, HID_KEY_NONE, NONE};
  layers[1].matrix[3][4] = {0, HID_KEY_SPACE, NONE};
  layers[1].encoders[0][0] = {0, HID_KEY_MINUS, NONE};
  layers[1].encoders[0][1] = {0, HID_KEY_CARET, NONE};
  layers[1].encoders[0][2] = {0, HID_KEY_NONE, NONE};
  layers[1].encoders[1][0] = {0, HID_KEY_COMMA, NONE};
  layers[1].encoders[1][1] = {0, HID_KEY_DOT, NONE};
  layers[1].encoders[1][2] = {0, HID_KEY_NONE, NONE};

  // Illustrator レイヤー設定 (Layer 2)
  strcpy(layers[2].layerName, "Illustrator");
  layers[2].ledR = 64;
  layers[2].ledG = 64;
  layers[2].ledB = 0; // 暗い黄
  layers[2].matrix[0][0] = {0, HID_KEY_ESCAPE, NONE};
  layers[2].matrix[0][1] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_C, NONE};
  layers[2].matrix[0][2] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_X, NONE};
  layers[2].matrix[0][3] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_V, NONE};
  layers[2].matrix[0][4] = {0, KEY_MODE_CHANGE, NONE};
  layers[2].matrix[1][0] = {0, HID_KEY_I, NONE};
  layers[2].matrix[1][1] = {0, HID_KEY_P, NONE};
  layers[2].matrix[1][2] = {KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_C, NONE};
  layers[2].matrix[1][3] = {KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_KP_PLUS, NONE};
  layers[2].matrix[1][4] = {0, HID_KEY_MINUS, NONE};
  layers[2].matrix[2][0] = {KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_Z, NONE};
  layers[2].matrix[2][1] = {0, HID_KEY_A, NONE};
  layers[2].matrix[2][2] = {0, HID_KEY_V, NONE};
  layers[2].matrix[2][3] = {0, HID_KEY_H, NONE};
  layers[2].matrix[2][4] = {0, HID_KEY_NONE, MOUSE_R};
  layers[2].matrix[3][0] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_Z, NONE};
  layers[2].matrix[3][1] = {KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_NONE, NONE};
  layers[2].matrix[3][2] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_NONE, NONE};
  layers[2].matrix[3][3] = {KEYBOARD_MODIFIER_LEFTALT, HID_KEY_NONE, NONE};
  layers[2].matrix[3][4] = {0, HID_KEY_SPACE, NONE};
  layers[2].encoders[0][0] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_KP_PLUS, NONE};
  layers[2].encoders[0][1] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_KP_MINUS, NONE};
  layers[2].encoders[0][2] = {KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_0, NONE};
  layers[2].encoders[1][0] = {KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_H, NONE};
  layers[2].encoders[1][1] = {0, HID_KEY_V, NONE};
  layers[2].encoders[1][2] = {KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_1, NONE};
}
// ============================================
// Setup / Loop
// ============================================
void setup()
{
  led.begin();

  COLOR_OFF = led.Color(0, 0, 0);
  setLed(led.Color(255, 0, 0));

  Serial.begin(115200);

  // 初期値の代入（LittleFSにファイルが無い場合の一時退避用）
  initDefaultKeymaps();

  // LittleFSから前回保存したキーマップ設定を復元
  loadConfigFromFlash();

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
    setLed(led.Color(0, 255, 0));
    delay(150);
    setLed(COLOR_OFF);
    delay(150);
  }
  updateModeLED();
  Serial.println("Keyboard Ready");
}

void loop()
{
  // 0. シリアル通信経由のPC側アプリからの読み書き要求を処理
  handleSerialCommunication();

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