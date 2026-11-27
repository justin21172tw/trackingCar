# ESP32 智慧跟隨車

本專案使用 ESP32、MCP23017、八個超音波感測器及雙輪差速馬達。每顆感測器的 Trig 由 MCP23017 個別輸出，Echo 則直接接回 ESP32 獨立 GPIO。程式依序觸發八顆感測器、接收 Echo 脈衝並換算距離。

## 專案模組

```text
include/
  Config.h              全域腳位、感測器方向與運作參數
  MCP23017.h            MCP23017 I2C GPIO 驅動
  MotorController.h     雙輪差速馬達控制
  TargetSelector.h      從有效量測中選出最近目標及方向
  TargetFollower.h      依目標角度與距離操控差速馬達
  StandardizedTelemetry.h  版本化逐行 JSON 遙測格式與輸出器
  UltrasonicArray.h     八路超音波輪詢與距離量測
  UltrasonicArrayTest.h 八路超音波順序輪詢及 Monitor 狀態輸出
src/
  Config.cpp            全域 constexpr 陣列定義
  MCP23017.cpp          MCP23017 I2C GPIO 實作
  MotorController.cpp   馬達控制實作
  TargetSelector.cpp    最近有效超音波目標選擇
  TargetFollower.cpp    目標跟隨及馬達轉向決策
  UltrasonicArray.cpp   Trig、Echo 脈衝量測及距離換算
  main.cpp              初始化感測器並執行超音波目標跟隨
web/
  index.html            視覺化除錯介面
  styles.css            白底響應式介面樣式
  app.js                USB Web Serial、真實遙測與感測器圖表
```

## 方向名稱

以下左右均以車輛面向前方時的左右為準。感測器編號和接線依照這個順序固定對應：

| 縮寫 | 方向 |
|---|---|
| F | 前方 |
| FL | 前左 |
| FR | 前右 |
| LE | 左側 |
| RI | 右側 |
| R | 後方 |
| RL | 後左 |
| RR | 後右 |

超音波方位角依除錯網站 `web/app.js` 的安裝圖設定：F 0°、FL -42°、FR 42°、LE -90°、RI 90°、R 180°、RL -138°、RR 138°。正角度代表車輛右側，負角度代表左側。

## 接線前準備與注意事項

1. ESP32、MCP23017、超音波感測器及馬達驅動器的 **GND 必須共地**。
2. ESP32 GPIO 不可直接接收 5 V。若超音波模組以 5 V 供電，**每一條 Echo 線都要各自分壓**後再接 ESP32。
3. 建議 MCP23017 使用 3.3 V 供電，使 I2C 上拉電位與 ESP32 相容。SDA/SCL 不可被上拉至 5 V。
4. MCP23017 的 A0、A1、A2 接 GND 時，I2C 位址為 `0x20`；RESET 必須保持高電位。
5. GPIO 34、35、36、39 僅能作為輸入，這份配置只將它們用於 Echo。
6. 接線前請先斷開電源。MCP23017 封裝腳號以下以 28-pin 封裝為準；若使用模組板，請按板上的 `GPAx`、`SDA` 等標籤對照。

## 八顆超音波感測器接線表

每顆超音波模組通常有 `VCC`、`TRIG`、`ECHO`、`GND` 四個訊號。請按方向標記感測器，並依下表逐顆接線。

| 編號 | 方向 | 感測器 TRIG 接到 MCP23017 | MCP23017 封裝腳號 | 感測器 ECHO 經分壓後接到 ESP32 |
|---:|---|---|---:|---:|
| 0 | F | GPA0 | 21 | GPIO 26 |
| 1 | FL | GPA1 | 22 | GPIO 27 |
| 2 | FR | GPA2 | 23 | GPIO 32 |
| 3 | LE | GPA3 | 24 | GPIO 33 |
| 4 | RI | GPA4 | 25 | GPIO 34 |
| 5 | R | GPA5 | 26 | GPIO 35 |
| 6 | RL | GPA6 | 27 | GPIO 36 |
| 7 | RR | GPA7 | 28 | GPIO 39 |

每顆感測器的電源線另外接：

| 感測器接點 | 連接位置 |
|---|---|
| VCC | 感測器規格要求的電源；HC-SR04 通常接 5 V |
| GND | 系統共地 |
| TRIG | 對應表中的 MCP23017 GPA 輸出 |
| ECHO | 經分壓後接對應的 ESP32 GPIO |

### Echo 分壓接法範例

對每一顆輸出 5 V Echo 的感測器，各使用一組電阻：

```text
感測器 ECHO ── 1 kΩ ──┬── ESP32 對應 Echo GPIO
                       |
                      2 kΩ
                       |
系統 GND ──────────────┴── ESP32 GND
```

此接法將 5 V 降至約 3.3 V。八顆感測器需要八組分壓；不可把八條 Echo 合併，也不可讓 Echo 5 V 直接進入 ESP32。

## MCP23017 與 ESP32 接線表

若使用 28-pin MCP23017 晶片，依下表連接電源、I2C 和位址設定：

| MCP23017 封裝腳號 | MCP23017 訊號 | 連接位置 |
|---:|---|---|
| 9 | VDD | ESP32 3V3 |
| 10 | VSS / GND | 系統 GND |
| 12 | SCL | ESP32 GPIO 22 |
| 13 | SDA | ESP32 GPIO 21 |
| 15 | A0 | GND |
| 16 | A1 | GND |
| 17 | A2 | GND |
| 18 | RESET | 3V3；可用 10 kΩ 電阻上拉至 3V3 |
| 21–28 | GPA0–GPA7 | 依上方感測器表連接各顆 TRIG |
| 11、14 | NC | 不接 |
| 19、20 | INTA、INTB | 本專案未使用，可不接 |

SDA、SCL 需有上拉電阻至 **3.3 V**；若所用 MCP23017 模組板已內建上拉，確認上拉接至 3.3 V，不要再接到 5 V。

## ESP32 GPIO 一覽

這張表列出本專案使用的 ESP32 腳位，可用來檢查接線是否有重複或接錯：

| ESP32 GPIO | 方向 | 接到 |
|---:|---|---|
| 16 | 輸出 PWM | 馬達驅動器 B1A |
| 17 | 輸出 PWM | 馬達驅動器 B1B |
| 18 | 輸出 PWM | 馬達驅動器 A1A |
| 19 | 輸出 PWM | 馬達驅動器 A1B |
| 21 | 雙向 I2C 資料 | MCP23017 SDA（封裝腳 13） |
| 22 | 輸出 I2C 時鐘 | MCP23017 SCL（封裝腳 12） |
| 26 | 輸入 Echo | F 感測器 ECHO（分壓後） |
| 27 | 輸入 Echo | FL 感測器 ECHO（分壓後） |
| 32 | 輸入 Echo | FR 感測器 ECHO（分壓後） |
| 33 | 輸入 Echo | LE 感測器 ECHO（分壓後） |
| 34 | 輸入 Echo | RI 感測器 ECHO（分壓後） |
| 35 | 輸入 Echo | R 感測器 ECHO（分壓後） |
| 36 | 輸入 Echo | RL 感測器 ECHO（分壓後） |
| 39 | 輸入 Echo | RR 感測器 ECHO（分壓後） |

## 馬達驅動器接線

Motor A 預設為左輪，Motor B 預設為右輪。以下是 ESP32 到雙 H 橋驅動器的控制線。GPIO 16/17 的設定適用於未將這兩腳保留給 PSRAM 的 ESP32 開發板；若使用 ESP32-WROVER，請先確認模組是否使用 GPIO 16/17 作為 PSRAM。

| ESP32 GPIO | 馬達驅動器控制輸入 | 控制對象 |
|---:|---|---|
| 18 | A1A | 左輪 Motor A 輸入 A |
| 19 | A1B | 左輪 Motor A 輸入 B |
| 16 | B1A | 右輪 Motor B 輸入 A |
| 17 | B1B | 右輪 Motor B 輸入 B |

馬達接到驅動器的 A、B 馬達輸出端；馬達電源接驅動器規格允許的外部電源，不要由 ESP32 3V3 腳供電。馬達電源負極、驅動器 GND、ESP32 GND 必須相連。L9110S、HG7881 等模組的端子標示可能不同，請依模組上的 A/B 輸入與馬達輸出標籤確認。

## 基礎跟隨模式與 Monitor 訊息

`main.cpp` 每輪依序輪詢感測器 0～7，也就是 F、FL、FR、LE、RI、R、RL、RR；每顆感測器開始量測及取得結果時都會輸出一筆 Monitor 訊息。控制器目前將 150 cm 內最近的有效回波視為目標，依該感測器角度轉向，並在距離不大於 20 cm 時停止。這是基礎跟隨功能，尚未實作木板／靜態障礙物排除，初次上電請架高車輪並留意馬達動作。

Monitor 鮑率為 `115200`，訊息以易讀的英文狀態、方向、接線、距離與掃描摘要呈現，例如：

```text
ULTRASONIC ARRAY TEST: READY
Sensors: 8 | Poll order: F -> FL -> FR -> LE -> RI -> R -> RL -> RR
MCP23017: I2C 0x20 | SDA: GPIO 21 | SCL: GPIO 22
Minimum trigger interval: 60 ms | Motor: TARGET FOLLOWING ENABLED

--- Scan #1 started at 1234 ms | Order: F -> FL -> FR -> LE -> RI -> R -> RL -> RR ---
[1/8] F: Measuring...
[1/8] F | Trig: MCP23017 GPA0 (pin 0) | Echo: ESP32 GPIO 26 | Status: VALID | Distance: 42.3 cm
[2/8] FL | Trig: MCP23017 GPA1 (pin 1) | Echo: ESP32 GPIO 27 | Status: NO VALID READING | Reason: timeout or out of range
--- Scan #1 complete | Valid: 7/8 | No valid reading: 1/8 | Duration: 480 ms | Follow state: following | Target: S0 at 42.3 cm, 0.0 deg ---
```

每顆感測器都會列出方向、MCP23017 Trig 腳位、ESP32 Echo GPIO 和量測狀態；有效時顯示距離，無效時說明可能原因。每輪結束會列出有效數量、無效數量和耗時。相鄰 Trig 脈衝至少間隔 60 ms。

若 MCP23017 初始化失敗，Monitor 會每 2 秒重複顯示目前 I2C 位址、SDA/SCL GPIO 和檢查建議；即使 Monitor 比開機晚開啟，也能看到目前狀態。

## 視覺化除錯介面

韌體每完成一輪八顆感測器掃描，會在既有易讀文字訊息後額外輸出一行以 `@TELEMETRY ` 開頭的 JSON。解析器只需讀取此固定前綴的行，其他 `Serial` 訊息可直接忽略；資料結構與輸出器使用 Arduino `Print` 介面，USB 序列埠與未來的其他傳輸層可共用相同格式。此階段不包含 Bluetooth 傳輸。

以下為協定格式範例，不是即時讀值。網頁不載入範例值，也不產生模擬資料。

```json
@TELEMETRY {"v":1,"type":"scan","timestamp_ms":1234,"scan":1,"scan_duration_ms":480,"valid_count":7,"logic":{"state":"following","target_sensor_id":"S2","target_direction_deg":42.0,"target_distance_cm":33.1},"sensors":[{"id":"S0","direction":"F","distance_cm":42.3,"status":"valid","reason":null},{"id":"S1","direction":"FL","distance_cm":null,"status":"timeout","reason":"timeout"},{"id":"S2","direction":"FR","distance_cm":33.1,"status":"valid","reason":null},{"id":"S3","direction":"LE","distance_cm":81.6,"status":"valid","reason":null},{"id":"S4","direction":"RI","distance_cm":25.0,"status":"valid","reason":null},{"id":"S5","direction":"R","distance_cm":120.4,"status":"valid","reason":null},{"id":"S6","direction":"RL","distance_cm":61.7,"status":"valid","reason":null},{"id":"S7","direction":"RR","distance_cm":95.2,"status":"valid","reason":null}]}
```

完整訊息每輪都會帶有 S0～S7 八筆資料。`distance_cm` 單位為公分；有效讀值時 `status` 為 `valid` 且 `reason` 為 `null`，未收到 Echo 時為 `timeout`，超出設定量程時為 `invalid` 並以 `reason: "out_of_range"` 說明。`logic` 會回報跟隨狀態、最近目標感測器、方位角與距離；未找到目標時目標欄位為 `null`。前綴、schema 版本及欄位順序固定，資料值則隨每輪量測更新。

量測核心新增 `measureDetailed()` 供遙測區分逾時與超出量程；既有 `measureDistance()` 的有效距離／`-1.0f` 回傳行為維持不變。每輪仍依序測量 8 顆感測器，並維持相鄰觸發至少 60 ms；標準化資料在完整掃描後一次輸出，不額外延遲感測輪詢。

### 開啟網頁

1. 在 Chromium 系瀏覽器（例如 Chrome 或 Edge）開啟終端機，從專案根目錄執行：

   ```sh
   python -m http.server 8000 --directory web
   ```

2. 瀏覽 `http://localhost:8000`。未連接車輛時，介面顯示「No vehicle connected」，感測器讀值、圖表及掃描統計維持無資料；不會生成或顯示模擬值。直接開啟 `file://` 頁面不保證能使用 Web Serial；Web Serial 需要支援的瀏覽器及安全來源（localhost 或 HTTPS）。
3. 接上 ESP32 後按 **Connect USB** 並選擇序列埠。介面以 `115200` 鮑率讀取 USB，只處理 `@TELEMETRY ` 行；連線開啟後，在收到第一筆有效遙測前仍顯示無資料。頁面上的 USB diagnostics 會顯示瀏覽器實際收到的 bytes／lines、帶遙測前綴的行數、接受／拒絕筆數、最近一行原文及 USB VID/PID。Web Serial 不提供作業系統 COM 編號；若 bytes 為 0，請先確認所選 VID/PID 對應到開發板，並關閉 PlatformIO Serial Monitor 等占用同一序列埠的程式。
4. 收到韌體實際輸出的遙測後，才會更新距離、頻率、狀態與圖表。點選車體上的感測器或下方卡片可查看詳情；每顆圖表只保留最近 3 秒資料，無效讀值會形成曲線斷點，不會被畫成 0 cm。USB 中斷或手動斷線時會清除已顯示的讀值，避免將舊資料誤認為即時資料；連線中超過 2.5 秒未更新則標記為 stale。

跳動診斷依最近有效樣本的變化幅度判定；「Possible freeze」代表最近至少 2 秒讀值相同，是供除錯參考的啟發式提示，不等於硬體故障判定。感測器位置及車體輪廓是依韌體 F、FL、FR、LE、RI、R、RL、RR 順序固定的前端示意，可在 `web/app.js` 的 `SENSOR_CONFIGS` 調整座標與角度。

### 擴充方式

網頁接收器只依賴版本化遙測 JSON；Web Serial 只負責讀取 USB 字串行。未來如新增 Bluetooth 或其他傳輸，應將傳輸內容轉成相同遙測記錄後交給共用解析流程，不需改寫圖表與診斷 UI。現有跟隨決策會填入 `logic` 欄位，並由馬達控制器執行差速轉向。

## 編譯與上傳

使用 VS Code + PlatformIO 開啟專案，執行 Build 與 Upload。Serial Monitor 鮑率為 `115200`，會輸出初始化狀態及每輪超音波測試結果。
