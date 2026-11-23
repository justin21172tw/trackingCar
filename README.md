# ESP32 智慧跟隨車 (Tracking Car)

本專案為基於 **ESP32** 晶片開發之目標跟隨自走車。車體前方預計以擴充板連接 **八個超音波感測器**，透過依序輪詢測距的方式獲取目標機器人之方位角與相對距離，並透過雙輪差速馬達控制車身動態轉向並跟隨目標。

---

## 專案架構規劃 (三階段模組化設計)

本專案採物件導向與模組化分層設計，規劃為三大核心模組：

```
trackingCar/
├── platformio.ini              # PlatformIO 專案設定檔 (Arduino Framework)
├── README.md                   # 專案說明與硬體接線手冊
├── include/
│   ├── Config.h                # 系統全域設定 (腳位、物理參數、多路超音波映射)
│   ├── MCP23017.h              # [基礎] MCP23017 I2C 16-Bit I/O 擴充晶片驅動
│   ├── MotorController.h       # [模組一] 馬達驅動與差速控制標頭檔
│   ├── MotorTest.h             # [測試] 馬達自檢診斷
│   ├── UltrasonicSensor.h      # [模組二] 單一超音波測距標頭檔
│   ├── UltrasonicTest.h        # [測試] 單一超音波自檢診斷
│   ├── UltrasonicArray.h       # [模組三] 8路超音波陣列抽象化標頭檔 (CD74HC4067 + MCP23017)
│   └── UltrasonicArrayTest.h   # [測試] 8路超音波陣列自檢與診斷
└── src/
    ├── MCP23017.cpp            # MCP23017 I2C 實作
    ├── MotorController.cpp     # [模組一] 馬達驅動控制實作
    ├── UltrasonicSensor.cpp    # [模組二] 單一超音波測距實作
    ├── UltrasonicArray.cpp     # [模組三] 8路超音波陣列測距抽象實作
    └── main.cpp                # 測試與驗證主程式
```

| 模組編號 | 模組名稱 | 目前進度 | 功能描述 |
| :--- | :--- | :---: | :--- |
| **模組一** | **馬達操作邏輯 (MotorController)** | **完成** | 雙輪差速驅動、正反轉、差速轉向、原地旋轉、煞車制動、左右輪修偏校準。支援 2-Pin / 3-Pin 驅動晶片。 |
| **模組二** | **單一超音波測距 (UltrasonicSensor)** | **完成** | 封裝 Trig/Echo 觸發測量、防卡死超時保護 (Timeout)、中位數突波濾波、方位角設定與量測快取。 |
| **模組三** | **八路超音波測距抽象化 (UltrasonicArray)** | **完成** | 結合 CD74HC4067 (Echo MUX) 與 MCP23017 (Trig Expander)，抽象化「指定特定編號超音波進行一次測距」的底層時序與多工邏輯，支援任意非線性/動態輪詢。 |
| **模組四** | **動態追蹤與 PID 閉迴路控制** | *待開發* | 依據最後偵測目標感測器展開動態局部輪詢、目標角度向量解算、雙輪差速 PID 跟隨控制。 |

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
   - 傳統 `pulseIn` 若無指定超時會阻塞長達 1 秒。本模組會根據所設定的 `maxDistanceCm` 動態計算極限回波微秒數 (約 18ms)，超時立即中斷並返回 `-1.0f`，**確保後續 8 個超音波依序輪詢時絕不拖慢主迴圈**。
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

## 模組三：八路超音波測距抽象化 (`UltrasonicArray`)

### 晶片分工與硬體架構
- **CD74HC4067 (16:1 類比多路複用器)**：負責切換 8 顆超音波的 **Echo** 訊號 (使用通道 C0~C7)。
  - 原因：超音波測距依賴微秒級 (us) 的 `pulseIn()` 硬體計時。I2C 匯流排無法提供微秒級的實時訊號採樣；透過 CD74HC4067 類比開關，Echo 訊號直接以奈秒級延遲透傳至 ESP32 的單一硬體 GPIO（如 GPIO 34），兼具超高精確度與省腳位優點。
- **MCP23017 (16-Bit I2C I/O 擴充晶片)**：負責發射 8 顆超音波的 **Trig** 脈衝。
  - 8 個 Trig 腳位統一接在 MCP23017 的 GPA0~GPA7 (Pin 0~7)，完整使用 Port A，硬體配線更清晰單純。
  - CD74HC4067 的 S0~S3 頻道選擇腳位亦可直接接在 MCP23017 的 GPB4~GPB7 (Pin 12~15)，使整個 8 路超音波陣列**僅耗用 ESP32 3 根腳位**（SDA、SCL、Echo）！亦支援設定為 ESP32 GPIO 直控 S0~S3。

### 核心抽象 API (`measureDistance`)
本模組徹底抽象化「指定特定編號超音波進行一次測距」的底層時序與多工邏輯，**不強迫從 0~7 依序輪詢**，主程式可依據跟隨演算法自由決定呼叫順序：
```cpp
// 1. 初始化 (配置 I2C、MCP23017 與多工器頻道)
bool init();

// 2.【核心方法】指定特定超音波進行一次測距 (公分，超時或無回波返回 -1.0f)
float measureDistance(uint8_t sensorIndex);

// 3. 指定特定超音波進行多次採樣中位數濾波 (消除偶發環境突波)
float measureMedian(uint8_t sensorIndex, uint8_t samples = 3, uint8_t delayMs = 12);

// 4. 讀取快取值與感測器屬性
float getLastDistance(uint8_t sensorIndex) const;
uint32_t getLastMeasureTime(uint8_t sensorIndex) const;
bool isValid(uint8_t sensorIndex) const;
float getSensorAngle(uint8_t sensorIndex) const;
uint8_t getSensorCount() const;
```

### 硬體接線對應表 (`Config.h` -> `UltrasonicArrayPins`)
| 模組接口 | 晶片腳位 / 頻道 | ESP32 連接 | 備註 |
| :--- | :--- | :--- | :--- |
| **I2C SDA** | MCP23017 Pin 13 (SDA) | **GPIO 21** | 具備 4.7kΩ 上拉電阻 |
| **I2C SCL** | MCP23017 Pin 12 (SCL) | **GPIO 4** | 避開 Motor B1A (GPIO 22) 衝突 |
| **Echo 總線** | CD74HC4067 SIG (COM) | **GPIO 34** | 僅輸入腳位，安全抗干擾 |
| **MUX S0~S3** | CD74HC4067 S0~S3 | **MCP23017 GPB4~GPB7** | (亦可切換為 ESP32 直控模式) |
| **Trig 0 ~ 7**| Sensor 0~7 Trig | **MCP23017 GPA0~GPA7** | 8 個 Trig 觸發脈衝輸出 (Pin 0~7) |
| **Echo 0 ~ 7**| Sensor 0~7 Echo | **CD74HC4067 C0~C7** | 8 路類比開關導通至 SIG 總線 |

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
