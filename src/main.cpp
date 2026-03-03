#include <Arduino.h>
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// include 1D FX effects
#include "fx/1d/cylon.h"
#include "fx/1d/fire2012.h"
#include "fx/1d/noisewave.h"
#include "fx/1d/pacifica.h"
#include "fx/1d/pride2015.h"
#include "fx/1d/twinklefox.h"

using namespace fl;

// ========== WiFi 配置 ==========
#define TOY_SSID "funXled"  // WiFi名稱
#define TOY_PWD "12345678"    // WiFi密碼（至少8位）

// ========== LED配置 (ESP01S只有GPIO0和GPIO2可用) ==========
#define LED_PIN 0           // WS2812燈帶
#define NUM_LEDS 8          // 8個LED
#define COLOR_ORDER GRB     // WS2812色序
#define CHIPSET WS2812B     // LED晶片類型
CRGBArray<NUM_LEDS> leds;   // LED陣列

// ========== 震動感應器配置 ==========
#define VIBRATION_PIN 2     // 震動感應器
#define VIBRATION_THRESHOLD 600 // 震動觸發閾值 (根據實際情況調整)

// ========== 變數 ==========
unsigned long lastVibrationTime = 0;
int animationMode = 0;
bool autoMode = true;  // 自動模式（由震動觸發）
CRGB monoColor = CRGB::Cyan;  // 單色模式顏色

// additional constants for modes
#define MODE_RAINBOW 0
#define MODE_FLASH 1
#define MODE_BREATH 2
#define MODE_CHASE 3
#define MODE_CYLON 4
#define MODE_FIRE 5
#define MODE_NOISE 6
#define MODE_PACIFICA 7
#define MODE_PRIDE 8
#define MODE_TWINKLE 9
#define MODE_DEMO_RAINBOW 10
#define MODE_DEMO_GLITTER 11
#define MODE_DEMO_CONFETTI 12
#define MODE_DEMO_SINELON 13
#define MODE_DEMO_JUGGLE 14
#define MODE_DEMO_BPM 15
#define MODE_MONO 16
#define MODE_CLEARLED 17
#define MODE_COUNT 18 // 更新總模式數

// FX objects (created with NUM_LEDS)
Cylon cylon(NUM_LEDS);
Fire2012 fire2012(NUM_LEDS);
NoiseWave noiseWave(NUM_LEDS);
Pacifica pacifica(NUM_LEDS);
Pride2015 pride2015(NUM_LEDS);
TwinkleFox twinklefox(NUM_LEDS);

// demo pattern state
uint8_t demoHue = 0;


// ========== 呼吸燈模式（animationMode = 2）==========
CRGB breathingColors[] = {CRGB::Cyan, CRGB::Magenta, CRGB::Yellow, CRGB::Green, CRGB::Blue, CRGB::Red};
const int numBreathingColors = sizeof(breathingColors) / sizeof(breathingColors[0]);
int currentColorIndex = 0;           // 目前色彩索引
CRGB currentAnimColor = CRGB::Cyan;  // 目前動畫色彩（支援插值）
CRGB nextAnimColor = CRGB::Magenta;  // 下一個目標色彩
unsigned long breathingCycleCount = 0; // 呼吸循環次數
const unsigned long colorSwitchCycles = 3; // 每 3 個循環切換一次色彩
unsigned long colorTransitionFrames = 0; // 色彩漸層進度（每個 50ms 呼吸更新一次）
const unsigned long colorTransitionDuration = 20; // 色彩過渡持續 20 個呼吸週期（~1秒）

// ========== 閒置/睡眠管理 ==========
unsigned long lastActivity = 0;     // 最後活動時間（ms）
#define IDLETIMEOUT 180000           // 閒置超時 ms (預設 180000ms = 3 分鐘)

// ========== Web服務器 ==========
ESP8266WebServer server(80);

// ========== 函數聲明 ==========
void handleVibration();
void updateAnimation();
void breathingLight();
CRGB lerpColor(CRGB from, CRGB to, uint16_t t, uint16_t max_t);
void rainbowCycle(uint8_t brightness);
void randomFlash();
void chaseAnimation();

// demo patterns
void demoRainbow();
void demoRainbowGlitter();
void demoConfetti();
void demoSinelon();
void demoJuggle();
void demoBpm();

void setAnimationMode(int mode);
void initWiFi();
void handleRoot();
void handleAPI();
void handleSetMode();
void handleSetBrightness();
void handleSetColor();
void handleToggleAuto();
void resetIdleTimer();
void enterDeepSleep();

// ========== HTML前端 ==========
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-TW">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Interactive LED</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Arial', sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      padding: 20px;
    }
    .container {
      background: white;
      border-radius: 20px;
      padding: 30px;
      box-shadow: 0 10px 40px rgba(0,0,0,0.3);
      max-width: 400px;
      width: 100%;
      min-width: 0;
    }
    h1 { text-align: center; color: #333; margin-bottom: 30px; font-size: 28px; }
    .mode-section { margin-bottom: 25px; }
    .mode-section .section-title {
      display: none;
    }
    .section-title {
      font-size: 14px;
      color: #666;
      text-transform: uppercase;
      letter-spacing: 1px;
      margin-bottom: 12px;
      font-weight: bold;
    }
    .button-group {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
      margin-bottom: 15px;
      word-break: break-word;
      overflow-wrap: anywhere;
      max-width: 100%;
      box-sizing: border-box;
    }
    .button-group.full { grid-template-columns: 1fr; }
    button {
      padding: 12px 16px;
      border: none;
      border-radius: 8px;
      font-size: 14px;
      font-weight: bold;
      cursor: pointer;
      transition: all 0.3s ease;
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    .mode-btn {
      background: #f0f0f0;
      color: #333;
      border: 2px solid #ddd;
    }
    .mode-btn:hover { background: #e0e0e0; }
    .mode-btn.active {
      background: #667eea;
      color: white;
      border-color: #667eea;
    }
    .action-btn {
      background: #667eea;
      color: white;
    }
    .action-btn:hover { background: #5568d3; }
    .action-btn:active { transform: scale(0.98); }
    .control-section {
      background: #f9f9f9;
      padding: 15px;
      border-radius: 8px;
      margin-bottom: 15px;
    }
    .slider-group {
      display: flex;
      align-items: center;
      gap: 10px;
      margin-bottom: 10px;
    }
    .slider-group label { min-width: 60px; font-size: 15px; }
    input[type="range"] {
      flex: 1;
      height: 6px;
      border-radius: 3px;
      background: #ddd;
      outline: none;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 16px;
      height: 16px;
      border-radius: 50%;
      background: #667eea;
      cursor: pointer;
    }
    input[type="range"]::-moz-range-thumb {
      width: 16px;
      height: 16px;
      border-radius: 50%;
      background: #667eea;
      cursor: pointer;
      border: none;
    }
    .toggle {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 10px;
      background: white;
      border-radius: 6px;
      border: 1px solid #ddd;
    }
    .toggle-switch {
      width: 50px;
      height: 28px;
      background: #ccc;
      border-radius: 14px;
      position: relative;
      cursor: pointer;
      transition: background 0.3s;
    }
    .toggle-switch.active { background: #667eea; }
    .toggle-switch::after {
      content: '';
      position: absolute;
      width: 24px;
      height: 24px;
      background: white;
      border-radius: 50%;
      top: 2px;
      left: 2px;
      transition: left 0.3s;
    }
    .toggle-switch.active::after { left: 24px; }
    .lang-buttons {
      display: flex;
      gap: 8px;
      justify-content: center;
      margin-bottom: 20px;
    }
    .lang-btn {
      padding: 6px 12px;
      border: 1px solid #ddd;
      border-radius: 4px;
      background: #fff;
      cursor: pointer;
      font-size: 12px;
      font-weight: bold;
      transition: all 0.3s;
    }
    .lang-btn.active {
      background: #667eea;
      color: white;
      border-color: #667eea;
    }
    .lang-btn:hover { background: #f0f0f0; }
  </style>
</head>
<body>
  <div class="container">
    <div style="display: flex; align-items: center; justify-content: center; margin-bottom: 20px;">
      <svg width="160" height="50" viewBox="0 0 240 100" style="margin-right: 10px;">
        <!-- f (Blue) -->
        <text x="5" y="70" font-size="60" font-weight="bold" fill="#4169E1" font-family="Arial">f</text>
        <!-- u (Green) -->
        <text x="26" y="70" font-size="60" font-weight="bold" fill="#00CD00" font-family="Arial">u</text>
        <!-- n (Yellow) -->
        <text x="61" y="70" font-size="60" font-weight="bold" fill="#FFD700" font-family="Arial">n</text>
        <!-- X (Red) -->
        <text x="96" y="70" font-size="70" font-weight="bold" fill="#DC143C" font-family="Arial">X</text>
        <!-- edu (Black) -->
        <text x="142" y="70" font-size="60" font-weight="bold" fill="#000000" font-family="Arial">edu</text>
      </svg>
    </div>
    <div class="lang-buttons">
      <button class="lang-btn" onclick="setLanguage('en')">English</button>
      <button class="lang-btn" onclick="setLanguage('zh-TW')">繁體中文</button>
      <button class="lang-btn" onclick="setLanguage('zh-CN')">简体中文</button>
    </div>
        
    <div class="mode-section">
      <div class="section-title" id="modeTitle">Animation Mode</div>
      <div id="modeButtons" class="button-group" style="min-height: 50px;"></div>
    </div>
    
    <div id="controlPanel" class="control-section">
      <div class="slider-group">
        <label id="brightnessLabel">Brightness:</label>
        <input type="range" id="brightness" min="0" max="255" value="255" onchange="updateBrightness()">
        <span id="brightnessValue">255</span>
      </div>
    </div>
    
    <div id="colorPanel" class="control-section" style="display:none;">
      <div class="section-title" id="colorTitle">Color Selection</div>
      <div style="display: flex; gap: 10px; align-items: center;">
        <input type="color" id="colorPicker" value="#00ffff" onchange="updateColor()" style="width: 50px; height: 40px; border: none; border-radius: 4px; cursor: pointer;">
        <div style="flex: 1;">
          <div style="font-size: 12px; color: #666; margin-bottom: 5px;">R: <span id="colorR">0</span> G: <span id="colorG">255</span> B: <span id="colorB">255</span></div>
          <input type="text" id="colorHex" value="#00ffff" onchange="updateColorFromHex()" style="width: 100%; padding: 6px; border: 1px solid #ddd; border-radius: 4px; font-family: monospace; font-size: 12px;">
        </div>
      </div>
    </div>
    
    <div class="control-section" style="border-top: 1px solid #ddd; padding-top: 20px;">
      <div style="display: flex; align-items: center; gap: 10px; margin-bottom: 10px; justify-content: space-between;">
        <label id="vibrationLabel" style="margin: 0; flex: 1; font-size: 13px; font-weight: bold;"><span id="vibrationText">Vibration Trigger (Auto Mode)</span></label>
        <div class="toggle-switch" id="autoModeToggle" onclick="toggleAutoMode()" style="margin: 0;"></div>
      </div>
    </div>
    
    <div class="status" style="font-size: 12px;">
      <strong id="statusTitle">Status:</strong> <span id="statusConnected">Connected to WiFi</span><br>
    </div>
  </div>

  <script>
    var currentMode = 0;
    var currentLang = 'en';

    // list of translation keys for each animation mode in order
    var modeKeys = ['rainbowCycle','randomFlash','colorPulse','chase','cylon','fire','noise','pacifica','pride','twinkle','demoRainbow','demoGlitter','demoConfetti','demoSinelon','demoJuggle','demoBPM', 'mono', 'clearLEDs'];

    // Multi-language translations
    var i18n = {
      'en': {
        'title': 'Interactive LED',
        'animationMode': 'Animation Mode',
        'rainbow': 'Rainbow',
        'flash': 'Flash',
        'pulse': 'Breathing',
        'chase': 'Chase',
        'brightness': 'Brightness:',
        'colorTitle': 'Color Selection',
        'colorSelection': 'Color Selection',
        'manualControl': 'Manual Control',
        'mono': 'Mono',
        'vibrationTrigger': 'Vibration Trigger',
        'clearLEDs': 'Clear LEDs',
        'cleared': 'LEDs cleared!',
        'status': 'Status:',
        'statusLabel': 'Status: ',
        'currentMode': 'Mode: ',
        'connected': 'Connected',
        // mode names
        'rainbowCycle': 'Neon',
        'randomFlash': 'Random Flash',
        'colorPulse': 'Color Pulse',
        // fx names
        'cylon': 'Cylon',
        'fire': 'Fire',
        'noise': 'Noise Wave',
        'pacifica': 'Pacifica',
        'pride': 'Pride2015',
        'twinkle': 'Twinkle Fox',
        'demoRainbow': 'Rainbow',
        'demoGlitter': 'Rainbow+Glitter',
        'demoConfetti': 'Confetti',
        'demoSinelon': 'Sinelon',
        'demoJuggle': 'Juggle',
        'demoBPM': 'BPM',
        'close': 'Close'
      },
      'zh-TW': {
        'title': '互動LED玩具',
        'animationMode': '動畫模式',
        'rainbow': '彩虹',
        'flash': '閃爍',
        'pulse': '呼吸燈',
        'chase': '跑馬燈',
        'brightness': '亮度:',
        'colorTitle': '顏色選擇',
        'colorSelection': '顏色選擇',
        'manualControl': '手動控制',
        'mono': '單色',
        'vibrationTrigger': '震動觸發',
        'clearLEDs': '關閉LED',
        'cleared': 'LED已清空！',
        'status': '狀態:',
        'statusLabel': '狀態: ',
        'currentMode': '模式: ',
        'connected': '已連接',
        // mode names
        'rainbowCycle': '霓虹',
        'randomFlash': '隨機閃爍',
        'colorPulse': '色彩跳動',
        // fx names
        'cylon': '賽安隆',
        'fire': '火焰',
        'noise': '雜訊波',
        'pacifica': '太平洋',
        'pride': '驕傲彩虹',
        'twinkle': '閃爍狐狸',
        'demoRainbow': '彩虹',
        'demoGlitter': '彩虹+亮粉',
        'demoConfetti': '彩帶',
        'demoSinelon': '單點來回',
        'demoJuggle': '交錯',
        'demoBPM': '節拍',
        'close': '關閉'
      },
      'zh-CN': {
        'title': '互动LED玩具',
        'animationMode': '动画模式',
        'rainbow': '彩虹',
        'flash': '闪烁',
        'pulse': '呼吸灯',
        'chase': '跑马灯',
        'brightness': '亮度:',
        'colorTitle': '颜色选择',
        'colorSelection': '颜色选择',
        'manualControl': '手动控制',
        'mono': '单色',
        'vibrationTrigger': '振动触发',
        'clearLEDs': '关闭',
        'cleared': 'LED已清空！',
        'status': '状态:',
        'statusLabel': '状态: ',
        'currentMode': '模式: ',
        'connected': '已连接',
        // mode names
        'rainbowCycle': '霓虹',
        'randomFlash': '随机闪烁',
        'colorPulse': '色彩跳动',
        // fx names
        'cylon': '赛安隆',
        'fire': '火焰',
        'noise': '噪声波',
        'pacifica': '太平洋',
        'pride': '骄傲彩虹',
        'twinkle': '闪烁狐狸',
        'demoRainbow': '彩虹',
        'demoGlitter': '彩虹+亮片',
        'demoConfetti': '彩带',
        'demoSinelon': '單點往返',
        'demoJuggle': '抛球',
        'demoBPM': '节拍',
        'close': '关闭'
      }
    };
    
    // Get translation
    function t(key) {
      return i18n[currentLang][key] || i18n['en'][key] || key;
    }
    
    // helper: build the mode buttons dynamically
    function createModeButtons() {
      var container = document.getElementById('modeButtons');
      if (!container) return;
      container.innerHTML = '';
      modeKeys.forEach(function(key, idx) {
        var btn = document.createElement('button');
        btn.className = 'mode-btn' + (idx === currentMode ? ' active' : '');
        btn.id = 'modeBtn' + idx;
        btn.textContent = t(key);
        btn.setAttribute('data-mode', idx);
        btn.addEventListener('click', function() {
          setMode(idx);
        });
        container.appendChild(btn);
      });

    }

    // Update UI with current language
    function updateUI() {
      document.title = t('title');
      document.getElementById('modeTitle').textContent = t('animationMode');
      document.getElementById('brightnessLabel').textContent = t('brightness');
      document.getElementById('colorTitle').textContent = t('colorTitle');
      document.getElementById('vibrationText').textContent = t('vibrationTrigger') + ' (Auto Mode)';

      // rebuild/refresh mode buttons text
      createModeButtons();

      var langBtns = document.querySelectorAll('.lang-btn');
      for (var i = 0; i < langBtns.length; i++) {
        langBtns[i].classList.remove('active');
      }
      if (currentLang === 'en') langBtns[0].classList.add('active');
      else if (currentLang === 'zh-TW') langBtns[1].classList.add('active');
      else if (currentLang === 'zh-CN') langBtns[2].classList.add('active');
    }
    
    // Detect browser language and update
    function detectLanguage() {
      var browserLang = navigator.language || navigator.userLanguage;
      if (browserLang.indexOf('zh-Hans') !== -1 || browserLang === 'zh-CN') {
        currentLang = 'zh-CN';
      } else if (browserLang.indexOf('zh') !== -1) {
        currentLang = 'zh-TW';
      } else {
        currentLang = 'en';
      }
    }
    
    // Change language
    function setLanguage(lang) {
      currentLang = lang;
      updateUI();
      updateStatus(); // 語言切換時同步狀態
      // 高亮語言按鈕即時顯示
      setTimeout(function() {
        var langBtns = document.querySelectorAll('.lang-btn');
        langBtns.forEach(btn => btn.classList.remove('active'));
        if (lang === 'en') langBtns[0].classList.add('active');
        else if (lang === 'zh-TW') langBtns[1].classList.add('active');
        else if (lang === 'zh-CN') langBtns[2].classList.add('active');
      }, 0);
    }
    
    function setMode(mode) {
      currentMode = mode;
      fetch('/api/setMode?mode=' + mode)
        .then(function(r) { return r.json(); })
        .then(function(data) {
          // update status label
          var modes = modeKeys.map(function(k){ return t(k); });
          // toggle active button
          modeKeys.forEach(function(_, idx) {
            var btn = document.getElementById('modeBtn' + idx);
            if (btn) btn.classList.toggle('active', idx === mode);
          });
          // Mono顯示顏色面板
          var colorPanel = document.getElementById('colorPanel');
          if (modeKeys[mode] === 'mono') {
            colorPanel.style.display = '';
          } else {
            colorPanel.style.display = 'none';
          }
        })
        .catch(function(e) { console.error('Error setting mode:', e); });
    }
    
    function updateBrightness() {
      var val = document.getElementById('brightness').value;
      document.getElementById('brightnessValue').textContent = val;
      fetch('/api/setBrightness?value=' + val)
        .then(function(r) { return r.json(); })
        .catch(function(e) { console.log(e); });
    }
    
    function toggleAutoMode() {
      fetch('/api/toggleAuto')
        .then(function(r) { return r.json(); })
        .then(function(data) {
          console.log('toggleAuto response:', data);
          updateStatus();
        });
    }
    
    function updateAutoModeToggle(active) {
      var toggle = document.getElementById('autoModeToggle');
      if (toggle) {
        toggle.classList.toggle('active', active);
      }
    }

    function updateStatus() {
      fetch('/api/status')
        .then(r => r.json())
        .then(data => {
          document.getElementById('statusConnected').textContent = data.autoMode ? t('connected') : '';
          // 按鈕同步
          currentMode = data.mode;
          var buttons = document.querySelectorAll('.mode-btn');
          buttons.forEach((btn, idx) => {
            btn.classList.toggle('active', idx === currentMode);
          });
          // Mono顯示顏色面板
          var colorPanel = document.getElementById('colorPanel');
          if (modeKeys[currentMode] === 'mono') {
            colorPanel.style.display = '';
          } else {
            colorPanel.style.display = 'none';
          }
          // 震動開關同步
          updateAutoModeToggle(data.autoMode);
        });
    }

    // Color functions
    function hexToRgb(hex) {
      var result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
      return result ? {
        r: parseInt(result[1], 16),
        g: parseInt(result[2], 16),
        b: parseInt(result[3], 16)
      } : { r: 0, g: 255, b: 255 };
    }
    
    function rgbToHex(r, g, b) {
      return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
    }
    
    function updateColor() {
      var hex = document.getElementById('colorPicker').value;
      var rgb = hexToRgb(hex);
      document.getElementById('colorHex').value = hex;
      document.getElementById('colorR').textContent = rgb.r;
      document.getElementById('colorG').textContent = rgb.g;
      document.getElementById('colorB').textContent = rgb.b;
      
      // Send to device
      fetch('/api/setColor?r=' + rgb.r + '&g=' + rgb.g + '&b=' + rgb.b)
        .then(function(r) { return r.json(); })
        .catch(function(e) { console.log(e); });
    }
    
    function updateColorFromHex() {
      var hex = document.getElementById('colorHex').value;
      if (hex.startsWith('#') && hex.length === 7) {
        document.getElementById('colorPicker').value = hex;
        updateColor();
      }
    }
    
    window.onload = function() {
      console.log('Page loaded');
      if (typeof modeKeys === 'undefined' || !modeKeys) {
        console.error('modeKeys not defined');
      } else {
        console.log('modeKeys defined:', modeKeys.length);
      }
      createModeButtons();
      detectLanguage();
      updateUI();
      updateStatus();
      // Update status every 2 seconds
      setInterval(updateStatus, 2000);
    };
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========== 互動LED初始化 ==========");
  
  // WiFi初始化
  initWiFi();
  
  // Web服務器路由
  server.on("/", handleRoot);
  server.on("/api/status", handleAPI);
  server.on("/api/setMode", handleSetMode);
  server.on("/api/setBrightness", handleSetBrightness);
  server.on("/api/setColor", handleSetColor);
  server.on("/api/toggleAuto", handleToggleAuto);
  server.begin();
  
  Serial.println("🚀 Web服務器已啟動");
  Serial.print("📱 訪問: http://");
  Serial.print(WiFi.softAPIP());
  Serial.println("/");
  Serial.println("===================================\n");

  // LED初始化
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(255);
  FastLED.setDither(BINARY_DITHER);
  FastLED.clear();
  FastLED.show();
  
  // 震動感應器初始化
  pinMode(VIBRATION_PIN, INPUT_PULLUP);
  // 初始化閒置計時
  resetIdleTimer();
}

void loop() {
  // 處理Web請求
  server.handleClient();
  
  // 檢測震動
  if (autoMode && digitalRead(VIBRATION_PIN) == LOW) { // 震動感應器通常是常高，震動時拉低
    unsigned long currentTime = millis();
    if (currentTime - lastVibrationTime > VIBRATION_THRESHOLD) {
      handleVibration();
      lastVibrationTime = currentTime;
    }
  }
  
  // 更新動畫
  updateAnimation();  
  FastLED.show();

  // 檢查是否閒置超時，進入深度睡眠
  if ((millis() - lastActivity) > IDLETIMEOUT) {
    Serial.println("🔌 閒置超時，準備進入深度睡眠...");
    enterDeepSleep();
  }
  delay(30);
}

void initWiFi() {
  Serial.println();
  Serial.println(F("Setting WiFi AP..."));
  WiFi.mode(WIFI_AP);
  delay(100); // 等待模式切換

  uint8_t macAddr[6];
  WiFi.softAPmacAddress(macAddr);
  char ssidBuffer[32];
  snprintf(ssidBuffer, sizeof(ssidBuffer), "%s_%02X%02X%02X", TOY_SSID, macAddr[3], macAddr[4], macAddr[5]);
  Serial.print(F("SSID=")); Serial.println(ssidBuffer);

  bool ok = WiFi.softAP(ssidBuffer, strlen(TOY_PWD) >= 8 ? TOY_PWD : nullptr);
  if (ok) {
    Serial.print(F("done! SSID=")); Serial.println(ssidBuffer);
  } else {
    Serial.println(F("softAP (open) 失敗，請檢查硬體連線與引腳狀態"));
    Serial.print(F("WiFi mode: ")); Serial.println(WiFi.getMode());
    Serial.print(F("WiFi status: ")); Serial.println(WiFi.status());
  }
}

void handleRoot() {
  resetIdleTimer();
  server.send(200, "text/html; charset=utf-8", htmlPage);
}

void handleAPI() {
  resetIdleTimer();
  String response = "{\"status\":\"ok\",\"mode\":" + String(animationMode) + ",\"autoMode\":" + String(autoMode ? "true" : "false") + "}";
  server.send(200, "application/json", response);
}

void handleSetMode() {
  resetIdleTimer();
  if (server.hasArg("mode")) {
    int mode = server.arg("mode").toInt();
    setAnimationMode(mode);
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    server.send(400, "application/json", "{\"error\":\"缺少參數\"}" );
  }
}

void handleSetBrightness() {
  resetIdleTimer();
  if (server.hasArg("value")) {
    int brightness = server.arg("value").toInt();
    // 範圍校驗：0-255
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
    FastLED.setBrightness(brightness);
    //FastLED.show();
    Serial.print("💡 亮度設置: ");
    Serial.println(brightness);
    server.send(200, "application/json", "{\"status\":\"ok\",\"brightness\":" + String(brightness) + "}");
  } else {
    server.send(400, "application/json", "{\"error\":\"missing value parameter\"}");
  }
}

void handleSetColor() {
  resetIdleTimer();
  if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) {
    int r = constrain(server.arg("r").toInt(), 0, 255);
    int g = constrain(server.arg("g").toInt(), 0, 255);
    int b = constrain(server.arg("b").toInt(), 0, 255);
    monoColor = CRGB(r, g, b);
    Serial.print("🎨 顏色設置 RGB(");
    Serial.print(r); Serial.print(",");
    Serial.print(g); Serial.print(",");
    Serial.print(b); Serial.println(")");
    server.send(200, "application/json", "{\"status\":\"ok\",\"color\":\"rgb(" + String(r) + "," + String(g) + "," + String(b) + ")\"}");
  } else {
    server.send(400, "application/json", "{\"error\":\"missing color parameters\"}");
  }
}

void handleToggleAuto() {
  resetIdleTimer();
  autoMode = !autoMode;
  Serial.print("🔄 自動模式: ");
  Serial.println(autoMode ? "啟用" : "禁用");
  server.send(200, "application/json", "{\"status\":\"ok\",\"autoMode\":" + String(autoMode ? "true" : "false") + "}");
}

void setAnimationMode(int mode) {
  animationMode = mode;
  // 重設色彩相關變數
  currentColorIndex = 0;
  currentAnimColor = breathingColors[0];
  nextAnimColor = breathingColors[1];
  breathingCycleCount = 0;
  colorTransitionFrames = 0;
  
  Serial.print("📺 模式切換: ");
  
  switch(mode) {
    case MODE_RAINBOW: Serial.println("彩虹循環"); break;
    case MODE_FLASH: Serial.println("隨機閃爍"); break;
    case MODE_BREATH: Serial.println("呼吸燈"); break;
    case MODE_CHASE: Serial.println("跑馬燈"); break;
    case MODE_CYLON: Serial.println("Cylon"); break;
    case MODE_FIRE: Serial.println("Fire2012"); break;
    case MODE_NOISE: Serial.println("Noise Wave"); break;
    case MODE_PACIFICA: Serial.println("Pacifica"); break;
    case MODE_PRIDE: Serial.println("Pride2015"); break;
    case MODE_TWINKLE: Serial.println("TwinkleFox"); break;
    case MODE_DEMO_RAINBOW: Serial.println("Rainbow"); break;
    case MODE_DEMO_GLITTER: Serial.println("Rainbow+Glitter"); break;
    case MODE_DEMO_CONFETTI: Serial.println("Confetti"); break;
    case MODE_DEMO_SINELON: Serial.println("Sinelon"); break;
    case MODE_DEMO_JUGGLE: Serial.println("Juggle"); break;
    case MODE_DEMO_BPM: Serial.println("BPM"); break;
    case MODE_MONO: Serial.println("單色"); break;
    case MODE_CLEARLED: Serial.println("清空LED"); break;
    default: Serial.println("未知模式");
  }
}

void handleVibration() {
  resetIdleTimer();
  Serial.println("✨ 偵測到震動！");
  animationMode = (animationMode + 1) % MODE_COUNT;
}

void updateAnimation() {
  switch(animationMode) {
    case MODE_RAINBOW:
      rainbowCycle(255);
      break;
    case MODE_FLASH:
      randomFlash();
      break;
    case MODE_BREATH:
      breathingLight();
      break;
    case MODE_CHASE:
      chaseAnimation();
      break;
    case MODE_CYLON:
      cylon.draw(fl::Fx::DrawContext(millis(), leds));
      break;
    case MODE_FIRE:
      fire2012.draw(fl::Fx::DrawContext(millis(), leds));
      break;
    case MODE_NOISE:
      noiseWave.draw(fl::Fx::DrawContext(millis(), leds));
      break;
    case MODE_PACIFICA:
      pacifica.draw(fl::Fx::DrawContext(millis(), leds));
      break;
    case MODE_PRIDE:
      pride2015.draw(fl::Fx::DrawContext(millis(), leds));
      break;
    case MODE_TWINKLE:
      twinklefox.draw(fl::Fx::DrawContext(millis(), leds));
      break;
    case MODE_DEMO_RAINBOW:
      demoHue++;
      demoRainbow();
      break;
    case MODE_DEMO_GLITTER:
      demoHue++;
      demoRainbowGlitter();
      break;
    case MODE_DEMO_CONFETTI:
      demoHue++;
      demoConfetti();
      break;
    case MODE_DEMO_SINELON:
      demoHue++;
      demoSinelon();
      break;
    case MODE_DEMO_JUGGLE:
      demoHue++;
      demoJuggle();
      break;
    case MODE_DEMO_BPM:
      demoHue++;
      demoBpm();
      break;
    case MODE_MONO:
      for (int i = 0; i < NUM_LEDS; i++) leds[i] = monoColor;
      break;
    default:
      // unknown mode, just clear
      FastLED.clear();
      break;
  }
}

void rainbowCycle(uint8_t brightness) {
  static uint8_t hue = 0;
  hue += 3;
  
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV((hue + i * 255 / NUM_LEDS), 255, brightness);
  }
}

void randomFlash() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(random(255), random(255), random(255));
  }
}

void chaseAnimation() {
  static uint8_t position = 0;
  static unsigned long lastUpdate = 0;
  static uint8_t lastPosition = 255;  // 用來偵測一圈完成
  unsigned long now = millis();
  
  // 每100ms更新一次位置
  if (now - lastUpdate > 100) {
    lastPosition = position;
    position = (position + 1) % NUM_LEDS;
    lastUpdate = now;
    
    // 偵測完成一圈（從 NUM_LEDS-1 回到 0）
    if (lastPosition == NUM_LEDS - 1 && position == 0) {
      breathingCycleCount++;
      
      // 每 3 圈開始色彩過渡
      if (breathingCycleCount >= colorSwitchCycles && colorTransitionFrames == 0) {
        int nextIdx = (currentColorIndex + 1) % numBreathingColors;
        nextAnimColor = breathingColors[nextIdx];
        colorTransitionFrames = 1;  // 開始過渡
        breathingCycleCount = 0;
      }
    }
  }
  
  // 清空所有LED
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  
  // 計算當前顯示色彩（支援漸層過渡）
  CRGB chaseColor = currentAnimColor;
  if (colorTransitionFrames > 0) {
    if (colorTransitionFrames < colorTransitionDuration) {
      chaseColor = lerpColor(currentAnimColor, nextAnimColor, colorTransitionFrames, colorTransitionDuration);
      colorTransitionFrames++;
    } else {
      // 過渡完成
      currentColorIndex = (currentColorIndex + 1) % numBreathingColors;
      currentAnimColor = nextAnimColor;
      colorTransitionFrames = 0;
      chaseColor = currentAnimColor;
    }
  }
  
  // 使用當前色彩（支援漸層）繪製跑馬燈
  leds[position] = chaseColor;
  if (position > 0) leds[position - 1] = CRGB(chaseColor.r / 2, chaseColor.g / 2, chaseColor.b / 2);
  if (position > 1) leds[position - 2] = CRGB(chaseColor.r / 4, chaseColor.g / 4, chaseColor.b / 4);
  if (position > 2) leds[position - 3] = CRGB(chaseColor.r / 8, chaseColor.g / 8, chaseColor.b / 8);
}

// --- demo pattern implementations ---

void demoRainbow() {
  fill_rainbow(leds, NUM_LEDS, demoHue, 7);
}

void addDemoGlitter(uint8_t chance) {
  if (random8() < chance) {
    leds[random16(NUM_LEDS)] += CRGB::White;
  }
}

void demoRainbowGlitter() {
  demoRainbow();
  addDemoGlitter(80);
}

void demoConfetti() {
  fadeToBlackBy(leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV(demoHue + random8(64), 200, 255);
}

void demoSinelon() {
  fadeToBlackBy(leds, NUM_LEDS, 20);
  int pos = beatsin16(13, 0, NUM_LEDS - 1);
  leds[pos] += CHSV(demoHue, 255, 192);
}

void demoJuggle() {
  fadeToBlackBy(leds, NUM_LEDS, 20);
  uint8_t dothue = 0;
  for (uint8_t i = 0; i < 8; i++) {
    leds[beatsin16(i + 7, 0, NUM_LEDS - 1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void demoBpm() {
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette(palette, demoHue + (i * 2), beat - demoHue + (i * 10));
  }
}

// 呼吸燈模式：平滑呼吸，每 3 個循環平滑漸層換色
void breathingLight() {
  static uint8_t breathValue = 0;
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();
  
  // 每 50ms 更新一次呼吸亮度
  if (now - lastUpdate > 50) {
    breathValue += 4;  // 控制呼吸速度，數值越小越慢
    lastUpdate = now;
    
    // 偵測呼吸循環完成（breathValue 從 0 回到接近 0）
    if (breathValue % 256 < 4) {
      breathingCycleCount++;
      
      // 每 3 個循環開始色彩過渡
      if (breathingCycleCount >= colorSwitchCycles && colorTransitionFrames == 0) {
        int nextIdx = (currentColorIndex + 1) % numBreathingColors;
        nextAnimColor = breathingColors[nextIdx];
        colorTransitionFrames = 1;  // 開始過渡
        breathingCycleCount = 0;
      }
    }
    
    // 更新色彩過渡進度
    if (colorTransitionFrames > 0) {
      if (colorTransitionFrames < colorTransitionDuration) {
        colorTransitionFrames++;
      } else {
        // 過渡完成
        currentColorIndex = (currentColorIndex + 1) % numBreathingColors;
        currentAnimColor = nextAnimColor;
        colorTransitionFrames = 0;
      }
    }
  }
  
  // 計算當前顯示色彩（支援漸層過渡）
  CRGB displayColor = currentAnimColor;
  if (colorTransitionFrames > 0 && colorTransitionFrames < colorTransitionDuration) {
    displayColor = lerpColor(currentAnimColor, nextAnimColor, colorTransitionFrames, colorTransitionDuration);
  }
  
  // 利用 sin8 產生平滑呼吸曲線（0-255-0）
  uint8_t fade = sin8(breathValue);
  
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = displayColor;
    leds[i].nscale8(fade);
  }
}

// 色彩插值函數：平滑過渡從 from 色到 to 色
// t: 當前進度（0 ~ max_t），max_t: 最大進度
CRGB lerpColor(CRGB from, CRGB to, uint16_t t, uint16_t max_t) {
  if (t >= max_t) return to;
  if (t <= 0) return from;
  
  uint16_t ratio = (t * 256) / max_t;  // 0~256，256 表示 100% 到達目標色
  uint8_t r = (from.r * (256 - ratio) + to.r * ratio) / 256;
  uint8_t g = (from.g * (256 - ratio) + to.g * ratio) / 256;
  uint8_t b = (from.b * (256 - ratio) + to.b * ratio) / 256;
  
  return CRGB(r, g, b);
}

// 重設閒置計時（有使用者互動時呼叫）
void resetIdleTimer() {
  lastActivity = millis();
}

// 進入深度睡眠（等待外部 Reset / RST 喚醒）
void enterDeepSleep() {
  // 多重防抖檢測
  for(int i=0; i<15; i++) {
    if(digitalRead(VIBRATION_PIN) == LOW) {
      Serial.println("❌ 取消睡眠：檢測到持續震動");
      return;
    }
    delay(50);
  }
  Serial.println("✅ 震動檢測通過");

  // 關閉 LED
  FastLED.clear();
  FastLED.show();
  delay(50);
  Serial.println("✅ LED已關閉");

  // 停止Web服務器
  server.stop();
  delay(50);
  Serial.println("✅ Web服務器已停止");

  // 關閉 WiFi
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(50);
  Serial.println("✅ WiFi已關閉");

  Serial.println("💤 進入深度睡眠，需外部 Reset 喚醒");
  Serial.flush();
  // 呼叫深度睡眠，參數為 microseconds，0 表示無限期睡眠，需外部 Reset 喚醒
  ESP.deepSleep(0);

  // 如果仍執行到這裡，稍作等待
  delay(1000);
}