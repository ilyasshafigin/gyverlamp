#pragma once

#include <cstdint>
#include <cstring>

#include "Arduino.h"

// Host EEPROM shim for production EepromStore. It has an Arduino-like staged
// image and a separate persisted image so commit failure paths are testable.
class EepromClass {
public:
  uint16_t begin(uint16_t size) {
    _size = size <= capacity ? size : capacity;
    return _size;
  }

  uint8_t read(int address) const {
    if (address < 0 || address >= static_cast<int>(_size)) return 0;
    return _staged[address];
  }

  void write(int address, uint8_t value) {
    if (address < 0 || address >= static_cast<int>(_size)) return;
    _staged[address] = value;
  }

  template <typename T> void put(int address, const T& value) {
    if (address < 0 || address + static_cast<int>(sizeof(T)) > static_cast<int>(_size)) return;
    std::memcpy(&_staged[address], &value, sizeof(T));
  }

  template <typename T> void get(int address, T& value) const {
    if (address < 0 || address + static_cast<int>(sizeof(T)) > static_cast<int>(_size)) {
      std::memset(&value, 0, sizeof(T));
      return;
    }
    std::memcpy(&value, &_staged[address], sizeof(T));
  }

  bool commit() {
    ++_commitCount;
    if (_failNextCommit) {
      _failNextCommit = false;
      return false;
    }
    std::memcpy(_persisted, _staged, sizeof(_persisted));
    return true;
  }

  uint16_t length() const { return _size; }

  void reset() {
    _size = capacity;
    std::memset(_staged, 0, sizeof(_staged));
    std::memset(_persisted, 0, sizeof(_persisted));
    _failNextCommit = false;
    _commitCount = 0;
  }

  void loadPersisted(const uint8_t* image, uint16_t size) {
    reset();
    const uint16_t copySize = size <= capacity ? size : capacity;
    std::memcpy(_persisted, image, copySize);
    std::memcpy(_staged, _persisted, sizeof(_staged));
  }

  void copyPersisted(uint8_t* output, uint16_t size) const {
    const uint16_t copySize = size <= capacity ? size : capacity;
    std::memcpy(output, _persisted, copySize);
  }

  void copyStaged(uint8_t* output, uint16_t size) const {
    const uint16_t copySize = size <= capacity ? size : capacity;
    std::memcpy(output, _staged, copySize);
  }

  uint8_t persistedAt(int address) const {
    if (address < 0 || address >= static_cast<int>(capacity)) return 0;
    return _persisted[address];
  }

  void failNextCommit() { _failNextCommit = true; }
  uint32_t commitCount() const { return _commitCount; }

private:
  static constexpr uint16_t capacity = 512;
  uint16_t _size = capacity;
  uint8_t _staged[capacity] = {};
  uint8_t _persisted[capacity] = {};
  bool _failNextCommit = false;
  uint32_t _commitCount = 0;
};

inline EepromClass EEPROM;
