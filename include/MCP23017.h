#pragma once

#include <Arduino.h>
#include <Wire.h>

/**
 * ==============================================================================
 * Tracking Car - MCP23017 16-Bit I2C I/O 擴充晶片驅動 (MCP23017.h)
 * ==============================================================================
 * 提供輕量化的 MCP23017 I2C GPIO 控制；超音波陣列使用 Port A 輸出 Trig 脈衝。
 */

class MCP23017 {
public:
    static constexpr uint8_t DEFAULT_I2C_ADDR = 0x20; // A0=GND, A1=GND, A2=GND

    // 暫存器位址 (當 IOCON.BANK = 0 時之預設交錯排列)
    enum Registers : uint8_t {
        REG_IODIRA   = 0x00, // Port A 方向暫存器 (1=輸入, 0=輸出)
        REG_IODIRB   = 0x01, // Port B 方向暫存器 (1=輸入, 0=輸出)
        REG_IPOLA    = 0x02, // Port A 輸入極性反轉
        REG_IPOLB    = 0x03, // Port B 輸入極性反轉
        REG_GPINTENA = 0x04, // Port A 中斷使能
        REG_GPINTENB = 0x05, // Port B 中斷使能
        REG_DEFVALA  = 0x06, // Port A 預設比較值
        REG_DEFVALB  = 0x07, // Port B 預設比較值
        REG_INTCONA  = 0x08, // Port A 中斷控制
        REG_INTCONB  = 0x09, // Port B 中斷控制
        REG_IOCON    = 0x0A, // 組態暫存器
        REG_GPPUA    = 0x0C, // Port A 上拉電阻 (1=啟用, 0=停用)
        REG_GPPUB    = 0x0D, // Port B 上拉電阻 (1=啟用, 0=停用)
        REG_INTFA    = 0x0E, // Port A 中斷旗標
        REG_INTFB    = 0x0F, // Port B 中斷旗標
        REG_INTCAPA  = 0x10, // Port A 中斷擷取值
        REG_INTCAPB  = 0x11, // Port B 中斷擷取值
        REG_GPIOA    = 0x12, // Port A 邏輯狀態讀寫
        REG_GPIOB    = 0x13, // Port B 邏輯狀態讀寫
        REG_OLATA    = 0x14, // Port A 輸出閂鎖
        REG_OLATB    = 0x15  // Port B 輸出閂鎖
    };

    /**
     * @brief 建構子
     * @param i2cAddr I2C 設備位址 (預設 0x20)
     */
    explicit MCP23017(uint8_t i2cAddr = DEFAULT_I2C_ADDR);

    /**
     * @brief 初始化 I2C 通訊並配置 MCP23017
     * @param sdaPin I2C SDA 腳位 (-1 表示使用系統預設)
     * @param sclPin I2C SCL 腳位 (-1 表示使用系統預設)
     * @param frequency I2C 傳輸頻率 (預設 400kHz Fast Mode)
     * @param wirePtr TwoWire 匯流排指標 (預設 &Wire)
     * @return true 裝置在線且初始化成功，false 失敗
     */
    bool begin(int8_t sdaPin = -1, int8_t sclPin = -1, uint32_t frequency = 400000, TwoWire* wirePtr = &Wire);

    /**
     * @brief 檢測晶片是否在線上通訊正常
     */
    bool isConnected();

    /**
     * @brief 設定腳位模式 (pin: 0~15)
     * Pin 0~7 對應 GPA0~GPA7，Pin 8~15 對應 GPB0~GPB7
     * @param pin 腳位編號 (0 ~ 15)
     * @param mode INPUT, OUTPUT, 或 INPUT_PULLUP
     */
    void pinMode(uint8_t pin, uint8_t mode);

    /**
     * @brief 寫入腳位電位 (pin: 0~15)
     * @param pin 腳位編號 (0 ~ 15)
     * @param value HIGH 或 LOW
     */
    void digitalWrite(uint8_t pin, uint8_t value);

    /**
     * @brief 讀取腳位電位 (pin: 0~15)
     * @param pin 腳位編號 (0 ~ 15)
     * @return uint8_t HIGH 或 LOW
     */
    uint8_t digitalRead(uint8_t pin);

    /**
     * @brief 在指定腳位產生一個微秒級的高電位脈衝 (專為 Trig 發射優化)
     * @param pin 腳位編號 (0 ~ 15)
     * @param pulseWidthUs 脈衝寬度 (微秒，預設 10us)
     */
    void pulsePin(uint8_t pin, uint16_t pulseWidthUs = 10);

    /**
     * @brief 整組寫入 Port 8 位元輸出 (port: 0 為 Port A, 1 為 Port B)
     */
    void writePort(uint8_t port, uint8_t value);

    /**
     * @brief 一次性寫入完整 16-Bit GPIO (低 8 位為 Port A, 高 8 位為 Port B)
     */
    void writeGPIO(uint16_t value);

    /**
     * @brief 寫入暫存器
     */
    bool writeReg(uint8_t reg, uint8_t value);

    /**
     * @brief 讀取暫存器
     */
    uint8_t readReg(uint8_t reg);

private:
    uint8_t  _i2cAddr;
    TwoWire* _wire;
    uint8_t  _iodirA;
    uint8_t  _iodirB;
    uint8_t  _olatA;
    uint8_t  _olatB;
    uint8_t  _gppuA;
    uint8_t  _gppuB;
};
