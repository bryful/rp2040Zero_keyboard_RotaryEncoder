#pragma once

#define HID_KEY_NONE 0x00 // Delete (JIS: _ (同上))

#define HID_KEY_DELETE 0x4C	  // Delete (JIS: _ (同上))
#define HID_KEY_PAGEUP 0x4B	  // Delete (JIS: _ (同上))
#define HID_KEY_PAGEDOWN 0x4E // Delete (JIS: _ (同上))

#define HID_KEY_ARROWRIGHT 0x4F // Delete (JIS: _ (同上))
#define HID_KEY_ARROWLEFT 0x50	// Delete (JIS: _ (同上))
#define HID_KEY_ARROWDOWN 0x51	// Delete (JIS: _ (同上))
#define HID_KEY_ARROWUP 0x52	// Delete (JIS: _ (同上))

// JIS配列基準のHID Usage ID（Keyboard/Keypad Page 0x07）
#define HID_KEY_MINUS 0x2D // - =
// #define HID_KEY_YEN 0x89        // JIS: ¥ (バックスラッシュ位置)
#define HID_KEY_CARET 0x2E		// JIS: ^ (日本語キーボード独自)
#define HID_KEY_AT 0x2F			// JIS: @ 91
#define HID_KEY_LEFTBRACE 0x30	// JIS: [ 2F
#define HID_KEY_RIGHTBRACE 0x32 // JIS: ] 30
// #define HID_KEY_RIGHTBRACE2 0x32     // JIS: ] 30
#define HID_KEY_COLON 0x92	   // JIS: :
#define HID_KEY_SEMICOLON 0x33 // ;
// #define HID_KEY_APOSTROPHE 0x34      // '
// #define HID_KEY_GRAVE 0x35           // `
#define HID_KEY_COMMA 0x36			 // ,
#define HID_KEY_DOT 0x37			 // .
#define HID_KEY_SLASH 0x38			 // /
#define HID_KEY_UNDERSCORE 0x93		 // JIS: _
#define HID_KEY_KP_PLUS 0x57		 // Keypad +
#define HID_KEY_KP_MINUS 0x56		 // Keypad -
#define HID_KEY_KP_ASTERISK 0x55	 // Keypad *
#define HID_KEY_KP_SLASH 0x54		 // Keypad /
#define HID_KEY_KP_DOT 0x63			 // Keypad .
#define HID_KEY_KP_COMMA 0x85		 // JIS: Keypad ,
#define HID_KEY_KP_EQUAL 0x67		 // Keypad =
#define HID_KEY_KP_ENTER 0x58		 // Keypad Enter
#define HID_KEY_RO 0x87				 // JIS: ろ
#define HID_KEY_KANA 0x88			 // JIS: かな
#define HID_KEY_EISU 0x94			 // JIS: 英数
#define HID_KEY_HENKAN 0x8A			 // JIS: 変換
#define HID_KEY_MUHENKAN 0x8B		 // JIS: 無変換
#define HID_KEY_HIRAGANA 0x8C		 // JIS: ひらがな/カタカナ
#define HID_KEY_ZENKAKU_HANKAKU 0x8D // JIS: 全角/半角
#define HID_KEY_BACKSLASH 0x31		 // \ (US配列)
#define HID_KEY_PIPE 0x64			 // | (JIS: Shift+\)
#define HID_KEY_INTL1 0x87			 // JIS: ろ (同上)
#define HID_KEY_INTL2 0x89			 // JIS: ¥ (同上)
#define HID_KEY_INTL3 0x8A			 // JIS: 変換 (同上)
#define HID_KEY_INTL4 0x8B			 // JIS: 無変換 (同上)
#define HID_KEY_INTL5 0x8C			 // JIS: ひらがな/カタカナ (同上)
#define HID_KEY_INTL6 0x8D			 // JIS: 全角/半角 (同上)
#define HID_KEY_INTL7 0x94			 // JIS: 英数 (同上)
#define HID_KEY_INTL8 0x92			 // JIS: : (同上)
#define HID_KEY_INTL9 0x93			 // JIS: _ (同上)
