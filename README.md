<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="funxedu-logo4-white.png">
    <source media="(prefers-color-scheme: light)" srcset="funxedu-logo4-black.png">
    <img alt="funXedu" src="funxedu-logo4-black.png" style="max-width: 100%;">
  </picture>
  <br/>
</p>

# funXled - 互動LED玩具
<p>
  <strong>低成本、DIY（自己動手做）的多彩LED互動玩具項目，專為教育用途設計。</strong>
  <br/>
  <strong>大部分代碼由AI agent編寫</strong>
</p>

---

## 🎯 功能特點

- 多種多彩動畫效果
- 手機網頁無線控制
- 震動感應動畫切換
- 自動睡眠按鈕喚醒

### demo影片
[![](/docs/demo.png)](https://photos.app.goo.gl/DxF1x6WUieuEwjR69)

---

## 🔧 硬體需求

### 微控制器
| 項目 | 規格 |
|------|------|
| **主板** | ESP01S 或 ESP12S (ESP8266) |
| **GPIO** | GPIO0, GPIO2 (可用) |
| **工作電壓** | 3.3V |
| **Flash** | 1MB / 4MB |

### 電子元件
| 元件 | 規格 | 數量 |
|------|------|------|
| **LED 燈帶** | WS2812B (5V) | 8個LED |
| **震動感應器** | 平放時輸出高電位 | 1個 |
| **5V穩壓模組** | 1.8V-5V轉5V穩壓模組 | 1個 |
| **3.3V穩壓模組** | 5V轉3.3V穩壓模組 | 1個 |
| **電池** | 3.7V 鋰電池 | 1個 |
| **充電模組** | 1S USB充電模組 | 1個 |
| **撥動開關** | 小型 | 1個 |
| **按鈕開關** | 小型 | 1個 |

### 所需工具
* `燒錄器`: 用於燒錄韌體
* `USB 訊號線`: 用於程序下載
* `焊接工具`: 用於製作電路板
* `熱熔膠槍`: 用於固定電路板

### 連線圖
```
ESP01S (IO2) →  震動感應器 (訊號線)
ESP01S (IO0) →  WS2812B 燈帶 (DIN)
ESP01S (RST) →  按鈕開關 (+)
ESP01S (GND) →  穩壓模組 (GND) + 震動感應器 (GND) + 燈帶 (GND) + 充電模組 (-) + 鋰電池 (-) + 按鈕開關 (-)
ESP01S (3.3V) → 3.3V穩壓模組 (Vout) + 震動感應器 (VCC)
充電模組 (+) → 鋰電池 (+) + 撥動開關（+）
5V穩壓模組 (Vin)  → 撥動開關 (-)
5V穩壓模組 (Vout) → WS2812B 燈帶 (VCC) + 3.3V穩壓模組 (Vin)
```
![](/docs/0.jpg)

### 接線重點
- ESP8266 deep-sleep 只能由 Reset (RST) 或定時器喚醒；
- GPIO0 無法可靠作為 deep-sleep 喚醒來源，且在引導期間若被拉低會進入下載模式。
- 所有電源共地。
- 若 LED 使用 5V，資料訊號為 3.3V，通常可直接驅動短距離 WS2812；若遇不穩建議加電平位移。
- 先在麵包板上用按鍵（短按）驗證喚醒行為，再換成實際元件。

### 風險與注意
- 若震動開關是常低（水平放置時低電平），切勿直接接到 RST，因為這會在正常情況下持續將 RST 拉低，導致無法啟動。
- 為避免長時間被拉低，可加入微分電路（RC）或使用 transistor 產生短脈衝給 RST。
- 若需穩定且低功耗喚醒且需頻繁回應，建議改用支援外部 GPIO 喚醒功能的模組（例如 ESP32）。
---

## 💻 燒錄韌體

  * 下載韌體檔案 [ `ESP01S` ](firmware/firmware1m.bin) 或 [ `ESP12S` ](firmware/firmware4m.bin)
  * 將`ESP01S模組`正確放入 `燒錄器` 中 ![](firmware/programmer.jpg) (如果使用 `USB to TTL` 或 `開發板` 當燒錄器，請確保接線和電壓正確)
  * 以 `USB 訊號線` (不是充電線) 連接 `電腦` 和 `燒錄器` 
  * 點選進入 [`燒錄工具網頁版`](https://espressif.github.io/esptool-js/)
  * 點選網頁 `Connect` 按鈕，選擇正確的`USB端口` 後點選 `連線`
  * `Flash Address` 設為 `0x0`
  * 點選 `選擇檔案` 按鈕後選擇之前下載的韌體檔
  * 點選網頁 `Program` 按鈕後開始燒錄韌體
  * 燒錄完成後點選網頁 `Disconnect` 按鈕
  * 拔除 `USB` 線後取下 `ESP01S模組`

---

## 📱 使用說明

### 1️⃣ 自動模式
- 開啟開關，設備上電後進入自動模式
- 搖晃設備即可切換動畫效果
- 長時間無動作自動睡眠，搖晃喚醒


### 4️⃣ 無線控制模式
1. 搜尋 WiFi 網絡：`funXled_XXXXXX`，密碼：`12345678`
2. 在瀏覽器中訪問：`http://192.168.4.1/`

### 網頁介面
![](/docs/1.jpg)
- 點選按鈕切換效果
- **關閉LED** - 熄滅所有燈
- **亮度滑塊** - 0-255 級調節
- **震動觸發** - 震動感應開關
- **單色** - 點選後出現`色彩選擇器`可選擇燈光顏色
- **色彩選擇器** - RGB / 十六進制輸入 或 點選色板選擇顏色
![](/docs/2.jpg)

#### 語言選擇
支援英文、繁體中文、簡體中文 (自動檢測)


---

## 📊 文件結構

```
funXled/
├── firmware/                 # 韌體檔
├── src/main.cpp              # 主程序源檔
├── docs/                     # 說明檔附件
├── platformio.ini            # 配置文件
├── preview.html              # 獨立測試頁面
└── README.md                 # 本文檔
```

---
