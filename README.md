# RP2040 Zero Macro Keyboard + Dual Rotary Encoder

<img src=".\pict\a0.jpg"><br>

Waveshare RP2040 Zero 用の USB HID マクロキーボードです。
4x5 キーマトリクスと 2 基のロータリーエンコーダを入力として使い、
Photoshop / After Effects / Illustrator の 3 モードでショートカットを送信します。
<br>
3dpフォルダの中に3Dプリンター用のデータも入れてあります。

## 特徴

- 4x5 キーマトリクス（20 キー）
- ロータリーエンコーダ x2（回転 CW/CCW + 押し込み SW）
- 3 つのアプリ向けキーマップモード
- モード切替キー（%1）
- USB HID キーボード + マウス右クリック送信
- WS2812 (NeoPixel) 1 灯で状態表示
- 一定時間無操作時のマウスジグル
<img src=".\pict\a1.jpg"><br>
<img src=".\pict\a2.jpg"><br>
<img src=".\pict\a3.jpg"><br>
<img src=".\pict\a4.jpg"><br>

## ハードウェア

| 項目                 | 内容                                                       |
| -------------------- | ---------------------------------------------------------- |
| マイコン             | Waveshare RP2040 Zero                                      |
| フレームワーク       | Arduino (PlatformIO)                                       |
| USB                  | TinyUSB ベース HID（Keyboard/Mouse）                       |
| LED                  | WS2812 x1（GPIO16）                                        |
| キー入力             | 4x5 マトリクス（Rows: GPIO5,4,3,2 / Cols: GPIO6,7,8,9,10） |
| ロータリーエンコーダ | 2 基（A/B/SW それぞれ GPIO 割当あり）                      |

キー入力は INPUT_PULLUP 想定です（スイッチ押下で GND に落とす配線）。

## ピンアサイン

### キーマトリクス

| 種別 | GPIO           |
| ---- | -------------- |
| Row  | 5, 4, 3, 2     |
| Col  | 6, 7, 8, 9, 10 |

### ロータリーエンコーダ

| エンコーダ | CLK(A) | DT(B) | SW  |
| ---------- | ------ | ----- | --- |
| Encoder 1  | 11     | 12    | 13  |
| Encoder 2  | 14     | 15    | 26  |

### LED

| デバイス | GPIO |
| -------- | ---- |
| WS2812   | 16   |

## モードとキーマップ

モードは 3 つです。

1. Photoshop
2. After Effects
3. Illustrator

モード切替キー（%1）を押すと順番に切り替わります。

### 共通キー（全モード共通）

- ESC
- Ctrl+C
- Ctrl+X
- Ctrl+V
- Ctrl+Shift+Z（Redo）
- Ctrl+Z（Undo）
- Shift / Ctrl / Alt / Space
- Mouse Right Click

### ロータリーエンコーダ割り当て

#### Photoshop

- Enc0: CW = Ctrl+Keypad+, CCW = Ctrl+Keypad-, Push = Ctrl+0
- Enc1: CW = [, CCW = ], Push = B

#### After Effects

- Enc0: CW = -, CCW = ^, Push = なし
- Enc1: CW = ,, CCW = ., Push = なし

#### Illustrator

- Enc0: CW = Ctrl+Keypad+, CCW = Ctrl+Keypad-, Push = Ctrl+0
- Enc1: CW = Shift+H, CCW = V, Push = Ctrl+Shift+1

## LED 動作

- 起動時: 赤
- 起動完了時: 緑を点滅後にモード色へ遷移
- モード表示
  - Photoshop: 消灯
  - After Effects: 暗い青
  - Illustrator: 暗い黄

## スクリーンセーバー対策

一定時間入力がない場合、マウスポインタを小さく往復移動させるマウスジグルを実行します。

デフォルトは 3 分です。

```cpp
constexpr uint32_t SCREENSAVER_TIMEOUT_MS = 60000 * 3;
```

## ビルドと書き込み

### 必要環境

- VS Code
- PlatformIO 拡張
- RP2040 Zero（BOOTSEL モードで書き込み）

### PlatformIO 設定

- board: waveshare_rp2040_zero
- platform: raspberrypi
- framework: arduino
- upload_protocol: picotool
- lib_deps: Adafruit NeoPixel

### コマンド

```sh
pio run
pio run -t upload
pio device monitor -b 115200
```

## ディレクトリ構成

```text
.
|- platformio.ini
|- src/
|  |- main.cpp
|  |- USBHIDKeyboard_JIS.h
|- include/
|- lib/
|- test/
`- icons/
```

## 注意事項

- マトリクス配線はチャタリングやゴースト対策のため、必要に応じてダイオード追加を検討してください。
- 一部ショートカットは OS/アプリのキーボードレイアウト設定に依存します。
- HID キーコードの追加・調整は USBHIDKeyboard_JIS.h を編集してください。
