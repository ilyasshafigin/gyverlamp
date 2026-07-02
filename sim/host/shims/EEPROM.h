#pragma once

#include <cstdint>
#include <cstring>

#include "Arduino.h"

// Host EEPROM shim for production EepromStore. Mirrors the Arduino EEPROM API
// with an in-memory buffer so the simulator boots with firmware-like defaults
// without hardware.
class EepromClass {
 public:
  uint16_t begin(uint16_t size) {
    _size = size <= capacity ? size : capacity;
    return _size;
  }

  uint8_t read(int address) const {
    if (address < 0 || address >= static_cast<int>(_size)) return 0;
    return _data[address];
  }

  void write(int address, uint8_t value) {
    if (address < 0 || address >= static_cast<int>(_size)) return;
    _data[address] = value;
  }

  template <typename T>
  void put(int address, const T& value) {
    if (address < 0 || address + static_cast<int>(sizeof(T)) > static_cast<int>(_size)) return;
    std::memcpy(&_data[address], &value, sizeof(T));
  }

  template <typename T>
  void get(int address, T& value) const {
    if (address < 0 || address + static_cast<int>(sizeof(T)) > static_cast<int>(_size)) {
      std::memset(&value, 0, sizeof(T));
      return;
    }
    std::memcpy(&value, &_data[address], sizeof(T));
  }

  bool commit() { return true; }

  uint16_t length() const { return _size; }

 private:
  static constexpr uint16_t capacity = 512;
  uint16_t _size = capacity;
  uint8_t _data[capacity] = {};
};

inline EepromClass EEPROM;
