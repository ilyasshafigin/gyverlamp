#include "sim_audio_input.h"

#include <array>
#include <algorithm>
#include <cstddef>

namespace sim {
namespace {

constexpr uint8_t kAdcPinA0 = 0;
constexpr uint16_t kAdcMidpoint = 512;
constexpr uint16_t kAdcMax = 1023;
constexpr size_t kCapacity = 128;

std::array<uint16_t, kCapacity> buffer{};
size_t head = 0;
size_t count = 0;
bool enabled = false;
bool hadUnderrun = false;
uint32_t underrunCount = 0;
uint32_t overflowCount = 0;

void recordUnderrun() {
    hadUnderrun = true;
    ++underrunCount;
}

} // namespace

void audioPushSample(uint16_t sample) {
    if (!enabled) return;

    const uint16_t clamped = std::min(sample, kAdcMax);

    if (count == kCapacity) {
        head = (head + 1) % kCapacity;
        --count;
        ++overflowCount;
    }

    const size_t tail = (head + count) % kCapacity;
    buffer[tail] = clamped;
    ++count;
}

uint16_t audioReadAdc(uint8_t pin) {
    if (!enabled || pin != kAdcPinA0 || count == 0) {
        recordUnderrun();
        return kAdcMidpoint;
    }

    const uint16_t sample = buffer[head];
    head = (head + 1) % kCapacity;
    --count;
    return sample;
}

void audioSetEnabled(bool nextEnabled) {
    enabled = nextEnabled;
    if (!enabled) {
        audioFlush();
    }
}

bool audioEnabled() {
    return enabled;
}

bool audioHadUnderrun() {
    return hadUnderrun;
}

void audioClearUnderrun() {
    hadUnderrun = false;
}

void audioFlush() {
    head = 0;
    count = 0;
    hadUnderrun = false;
}

uint32_t audioUnderrunCount() {
    return underrunCount;
}

uint32_t audioOverflowCount() {
    return overflowCount;
}

} // namespace sim
