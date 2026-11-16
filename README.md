# ESP32 智慧跟隨車 (Tracking Car)

本專案為基於 **ESP32** 晶片開發之目標跟隨自走車。車體前方預計以擴充板連接 **十個超音波感測器**，透過依序輪詢測距的方式獲取目標機器人之方位角與相對距離，並透過雙輪差速馬達控制車身動態轉向並跟隨目標。

---

## 專案架構規劃 (三階段模組化設計)

本專案採物件導向與模組化分層設計，規劃為三大核心模組：

```
trackingCar/
├── platformio.ini              # PlatformIO 專案設定檔 (Arduino Framework)
├── README.md                   # 專案說明與硬體接線手冊
├── include/
│   ├── Config.h                # 系統全域設定 (腳位、物理參數、閥值)
│   ├── MotorController.h       # [模組一] 馬達驅動與差速控制標頭檔
│   └── UltrasonicSensor.h      # [模組二] 單一超音波測距標頭檔
└── src/
    ├── MotorController.cpp     # [模組一] 馬達驅動控制實作
    ├── UltrasonicSensor.cpp    # [模組二] 單一超音波測距實作
    └── main.cpp                # 測試與驗證主程式 (目前為前兩模組之整合示範)
```

| 模組編號 | 模組名稱 | 目前進度 | 功能描述 |
| :--- | :--- | :---: | :--- |
| **模組一** | **馬達操作邏輯 (MotorController)** | **完成** | 雙輪差速驅動、正反轉、差速轉向、原地旋轉、煞車制動、左右輪修偏校準。支援 2-Pin / 3-Pin 驅動晶片。 |
| **模組二** | **單一超音波測距 (UltrasonicSensor)** | **完成** | 封裝 Trig/Echo 觸發測量、防卡死超時保護 (Timeout)、中位數突波濾波、方位角設定與量測快取。 |
| **模組三** | **十個超音波輪詢與尋標追蹤** | *待開發* | 擴充板多感測器依序詢問、目標角度解算 (向量合成/加權平均)、PID 跟隨閉迴路控制。 |

---

## 模組一：馬達操作邏輯 (`MotorController`)

### 特色與設計亮點
1. **直覺對應 4 接口驅動橋 (A1A, A1B, B1A, B1B)**：
   - 專為 **L9110S**、**HG7881** 等 4 接口雙通道 H 橋模組優化，直驅雙路 PWM，精簡高效。
2. **軟體方向反轉支援 (`setInvert`)**：
   - 支援 `setInvert(invertA, invertB)`，若實體組裝時發現馬達轉向相反，可直接在程式中校正，免重新插拔線。
3. **左右輪修偏校準 (`setTrim`)**：
   - 可分別微調 Motor A / B 輸出比率 (0.0 ~ 2.0)，解決減速馬達製造公差導致的小車走偏問題。
4. **安全主動煞車 (`brake`) 與滑行停止 (`stop`)**：
   - 提供 H 橋雙端接 HIGH 的電氣短路制動 (`brake`) 與斷開輸出的滑行停止 (`stop`)。

### 常用 API 列表
```cpp
// 初始化
void init();

// 直行與後退 (速度 0 ~ 255)
void forward(uint8_t speed);
void backward(uint8_t speed);

// 差速轉向 (turnRatio 為內側輪速度係數，0.0~1.0)
void turnLeft(uint8_t speed, float turnRatio = 0.4f);
void turnRight(uint8_t speed, float turnRatio = 0.4f);

// 原地旋轉 (左右輪反向運轉)
void spinLeft(uint8_t speed);
void spinRight(uint8_t speed);

// 停止與煞車
void stop();  // 自然滑行
void brake(); // 主動緊急煞車

// 左右輪獨立控制 (-255 ~ 255)
void drive(int leftSpeed, int rightSpeed);

// 微調係數設定
void setTrim(float leftTrim, float rightTrim);
```

---

## 模組二：單一超音波測距 (`UltrasonicSensor`)

### 特色與設計亮點
1. **非阻塞安全 Timeout 保護**：
   - 傳統 `pulseIn` 若無指定超時會阻塞長達 1 秒。本模組會根據所設定的 `maxDistanceCm` 動態計算極限回波微秒數 (約 18ms)，超時立即中斷並返回 `-1.0f`，**確保後續 10 個超音波依序輪詢時絕不拖慢主迴圈**。
2. **具備方位角度標籤 (`angleOffset`)**：
   - 每個感測器實體皆可攜帶安裝方位角（如車頭 0°，側邊 ±45°、±90°），為第三模組的方向解算提供直接依據。
3. **中位數濾波器 (`measureMedian`)**：
   - 內建採樣中位數排序算法，有效濾除超音波在空氣中偶發的跳躍突波雜訊。

### 常用 API 列表
```cpp
// 初始化腳位
void init();

// 單次快速測距 (公分，無反射返回 -1.0f)
float measureDistance();

// 多次採樣中位數測距 (消雜訊)
float measureMedian(uint8_t samples = 3, uint8_t delayMs = 15);

// 讀取快取值與屬性
float getLastDistance() const;
uint32_t getLastMeasureTime() const;
bool isValid() const;
uint8_t getId() const;
float getAngleOffset() const;
```

---

## 硬體接線建議 (可於 `Config.h` 自訂修改)

> **注意**：ESP32 的 `GPIO 6~11` 為連接內建 SPI Flash 專用，切勿使用；`GPIO 34, 35, 36, 39` 為 Input Only，僅可接 Echo，不可接 Trig 或馬達。

### 1. 馬達驅動模組 (4 接口: A1A, A1B, B1A, B1B)
適用 L9110S / HG7881 等 4-Wire H 橋驅動模組：
| 模組接口標記 | 變數名稱 (`Config.h`) | 預設 ESP32 GPIO | 說明 |
| :--- | :--- | :--- | :--- |
| **A-1A (A1A)** | `PIN_A1A` | GPIO 18 | Motor A 控制端 A (PWM) |
| **A-1B (A1B)** | `PIN_A1B` | GPIO 19 | Motor A 控制端 B (PWM) |
| **B-1A (B1A)** | `PIN_B1A` | GPIO 22 | Motor B 控制端 A (PWM) |
| **B-1B (B1B)** | `PIN_B1B` | GPIO 23 | Motor B 控制端 B (PWM) |

> 提示：若實體組裝發現 Motor A 或 Motor B 旋轉方向相反，可直接在程式中呼叫 `motor.setInvert(invertA, invertB)`（例如 `motor.setInvert(true, false)`），毋須重新拔插接線。

### 2. 測試超音波模組 (預設配置)
| 訊號名稱 | ESP32 GPIO | 說明 |
| :--- | :--- | :--- |
| **TRIG_PIN** | GPIO 5 | 觸發訊號 (輸出) |
| **ECHO_PIN** | GPIO 17 | 回波接收 (輸入，注意 HC-SR04 5V 模組建議分壓至 3.3V) |

---

## 編譯與上傳方式

### 方式 A：使用 VS Code + PlatformIO (推薦)
1. 在 VS Code 開啟 `trackingCar` 資料夾。
2. PlatformIO 會自動辨識 `platformio.ini`。
3. 點選左下角「✓ (Build)」進行編譯，點選「→ (Upload)」燒錄至 ESP32。
4. 開啟 Serial Monitor (鮑率 `115200`) 即可檢視即時測距與馬達動作輸出。

### 方式 B：使用 Arduino IDE
1. 將 `include/` 及 `src/` 中的檔案加入同一個專案資料夾。
2. 選擇板子為 **ESP32 Dev Module**，鮑率設定 `115200`，直接點擊上傳。
