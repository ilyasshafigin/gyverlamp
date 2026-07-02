#pragma once

#include <cstdint>

namespace sim {

void audioPushSample(uint16_t sample);
uint16_t audioReadAdc(uint8_t pin);

void audioSetEnabled(bool enabled);
bool audioEnabled();

bool audioHadUnderrun();
void audioClearUnderrun();
void audioFlush();

uint32_t audioUnderrunCount();
uint32_t audioOverflowCount();

} // namespace sim
