#include "MCP23017.h"

MCP23017::MCP23017(uint8_t i2cAddr)
    : _i2cAddr(i2cAddr),
      _wire(&Wire),
      _iodirA(0xFF),
      _iodirB(0xFF),
      _olatA(0x00),
      _olatB(0x00),
      _gppuA(0x00),
      _gppuB(0x00) {}

bool MCP23017::begin(int8_t sdaPin, int8_t sclPin, uint32_t frequency, TwoWire* wirePtr) {
    if (wirePtr != nullptr) {
        _wire = wirePtr;
    }

    if (sdaPin >= 0 && sclPin >= 0) {
        _wire->begin(sdaPin, sclPin, frequency);
    } else {
        _wire->begin();
        _wire->setClock(frequency);
    }

    if (!isConnected()) {
        return false;
    }

    // 設定 IOCON: BANK=0, SEQOP=0, DISSLW=0, HAEN=0, ODR=0, INTPOL=0
    writeReg(REG_IOCON, 0x00);

    // 預設將 Port A 和 Port B 設為輸出 (0x00)
    _iodirA = 0x00;
    _iodirB = 0x00;
    writeReg(REG_IODIRA, _iodirA);
    writeReg(REG_IODIRB, _iodirB);

    // 關閉內部上拉電阻
    _gppuA = 0x00;
    _gppuB = 0x00;
    writeReg(REG_GPPUA, _gppuA);
    writeReg(REG_GPPUB, _gppuB);

    // 輸出預設拉低 LOW
    _olatA = 0x00;
    _olatB = 0x00;
    writeReg(REG_GPIOA, _olatA);
    writeReg(REG_GPIOB, _olatB);

    return true;
}

bool MCP23017::isConnected() {
    _wire->beginTransmission(_i2cAddr);
    return (_wire->endTransmission() == 0);
}

void MCP23017::pinMode(uint8_t pin, uint8_t mode) {
    if (pin < 8) {
        if (mode == INPUT || mode == INPUT_PULLUP) {
            _iodirA |= (1 << pin);
            if (mode == INPUT_PULLUP) {
                _gppuA |= (1 << pin);
            } else {
                _gppuA &= ~(1 << pin);
            }
            writeReg(REG_GPPUA, _gppuA);
        } else {
            _iodirA &= ~(1 << pin);
        }
        writeReg(REG_IODIRA, _iodirA);
    } else if (pin < 16) {
        uint8_t bit = pin - 8;
        if (mode == INPUT || mode == INPUT_PULLUP) {
            _iodirB |= (1 << bit);
            if (mode == INPUT_PULLUP) {
                _gppuB |= (1 << bit);
            } else {
                _gppuB &= ~(1 << bit);
            }
            writeReg(REG_GPPUB, _gppuB);
        } else {
            _iodirB &= ~(1 << bit);
        }
        writeReg(REG_IODIRB, _iodirB);
    }
}

void MCP23017::digitalWrite(uint8_t pin, uint8_t value) {
    if (pin < 8) {
        if (value) {
            _olatA |= (1 << pin);
        } else {
            _olatA &= ~(1 << pin);
        }
        writeReg(REG_GPIOA, _olatA);
    } else if (pin < 16) {
        uint8_t bit = pin - 8;
        if (value) {
            _olatB |= (1 << bit);
        } else {
            _olatB &= ~(1 << bit);
        }
        writeReg(REG_GPIOB, _olatB);
    }
}

uint8_t MCP23017::digitalRead(uint8_t pin) {
    if (pin < 8) {
        uint8_t val = readReg(REG_GPIOA);
        return (val & (1 << pin)) ? HIGH : LOW;
    } else if (pin < 16) {
        uint8_t val = readReg(REG_GPIOB);
        return (val & (1 << (pin - 8))) ? HIGH : LOW;
    }
    return LOW;
}

void MCP23017::pulsePin(uint8_t pin, uint16_t pulseWidthUs) {
    digitalWrite(pin, HIGH);
    if (pulseWidthUs > 0) {
        delayMicroseconds(pulseWidthUs);
    }
    digitalWrite(pin, LOW);
}

void MCP23017::writePort(uint8_t port, uint8_t value) {
    if (port == 0) {
        _olatA = value;
        writeReg(REG_GPIOA, _olatA);
    } else {
        _olatB = value;
        writeReg(REG_GPIOB, _olatB);
    }
}

void MCP23017::writeGPIO(uint16_t value) {
    _olatA = (uint8_t)(value & 0xFF);
    _olatB = (uint8_t)((value >> 8) & 0xFF);

    _wire->beginTransmission(_i2cAddr);
    _wire->write(REG_GPIOA);
    _wire->write(_olatA);
    _wire->write(_olatB);
    _wire->endTransmission();
}

bool MCP23017::writeReg(uint8_t reg, uint8_t value) {
    _wire->beginTransmission(_i2cAddr);
    _wire->write(reg);
    _wire->write(value);
    return (_wire->endTransmission() == 0);
}

uint8_t MCP23017::readReg(uint8_t reg) {
    _wire->beginTransmission(_i2cAddr);
    _wire->write(reg);
    _wire->endTransmission();

    _wire->requestFrom(_i2cAddr, (uint8_t)1);
    if (_wire->available()) {
        return _wire->read();
    }
    return 0;
}
