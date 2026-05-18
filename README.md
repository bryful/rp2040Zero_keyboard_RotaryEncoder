# RP2040 Zero キーボード

Waveshare RP2040 Zero を使ったカスタム USB HID キーボードファームウェアです。
最大14個のボタンを GPIO に接続し、キーコードを USB 経由でPCへ送信します。
スクリーンセーバー対策の自動 ESC 送信機能も搭載しています。

---

## ハードウェア

| 項目     | 内容                                            |
| -------- | ----------------------------------------------- |
| マイコン | Waveshare RP2040 Zero                           |
| USB      | USB HID キーボード (TinyUSB, arduino-pico 内蔵) |
| LED      | WS2812 RGB LED × 1（GPIO 16）                   |
| ボタン   | タクトスイッチ × 最大14個（GPIO 2〜15）         |
| 接続方式 | INPUT_PULLUP（GND 接地でON）                    |

---

## ピンアサイン（14キー構成）

| GPIO | キー                           | 種別     | LED色    |
| ---- | ------------------------------ | -------- | -------- |
| 2    | Space                          | 通常     | 黄       |
| 3    | Alt                            | 通常     | マゼンタ |
| 4    | Ctrl                           | 通常     | 緑       |
| 5    | Shift                          | 通常     | 黄       |
| 6    | Ctrl+V（ペースト）             | Ctrl複合 | 白       |
| 7    | Ctrl+C（コピー）               | Ctrl複合 | シアン   |
| 8    | Ctrl+テンキー+（ズームイン）   | Ctrl複合 | 青       |
| 9    | Ctrl+テンキー−（ズームアウト） | Ctrl複合 | 青       |
| 10   | Enter                          | 通常     | 白       |
| 11   | Ctrl+X（カット）               | Ctrl複合 | 赤       |
| 12   | `]`                            | 通常     | 緑       |
| 13   | `[`                            | 通常     | 緑       |
| 14   | Ctrl+Z（元に戻す）             | Ctrl複合 | 赤       |
| 15   | Escape                         | 通常     | 赤       |

---

## LED 動作

| 状態       | 色                |
| ---------- | ----------------- |
| 起動中     | 赤                |
| 起動完了   | 緑（1秒後に消灯） |
| キー押下中 | キー固有の色      |
| 無操作     | 消灯              |

---

## スクリーンセーバー対策

一定時間キー入力がなかった場合、自動的に **ESC キー** を押したことにして、スクリーンセーバーの起動を防ぎます。

**タイムアウト設定:**
`src/main.cpp` の下記定数を変更してください。

```cpp
constexpr uint32_t SCREENSAVER_TIMEOUT_MS = 60000; // 1分（ミリ秒）
```

---

## ビルド・書き込み

### 必要なもの

- [VS Code](https://code.visualstudio.com/) + [PlatformIO](https://platformio.org/) 拡張機能
- Waveshare RP2040 Zero
- USB ケーブル（Type-C）
- [picotool](https://github.com/raspberrypi/picotool)（書き込みに使用）

### 手順

1. VS Code でこのプロジェクトフォルダを開く
2. **ビルド:** PlatformIO の「Build」ボタン、または `platformio run`
3. **書き込み:** RP2040 Zero を BOOTSEL モードで接続し、「Upload」ボタン、または `platformio run --target upload`

---

## プロジェクト構成

```
platformio.ini        # PlatformIO 設定
src/
  main.cpp            # メインファームウェア
  USBHIDKeyboard_JIS.h  # JIS キーボード用 HID キーコード定義
```

---

## 使用ライブラリ

| ライブラリ                   | 用途                 |
| ---------------------------- | -------------------- |
| arduino-pico (内蔵 Keyboard) | USB HID キーボード   |
| Adafruit NeoPixel            | WS2812 LED 制御      |
| TinyUSB (arduino-pico 内蔵)  | USB HID レポート送信 |
