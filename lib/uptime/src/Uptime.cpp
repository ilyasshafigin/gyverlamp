/* ***********************************************************************
 * Uptime library for Arduino boards and compatible systems
 * (C) 2019 by Yiannis Bourkelis (https://github.com/YiannisBourkelis/)
 *
 * This file is part of Uptime library for Arduino boards and compatible systems
 *
 * Uptime library for Arduino boards and compatible systems is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Uptime library for Arduino boards and compatible systems is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Uptime library for Arduino boards and compatible systems.  If not, see <http://www.gnu.org/licenses/>.
 * ***********************************************************************/

/*
 * Uptime library for Arduino boards and compatible systems
 *
 * Caclulates the time passed since the device boot time, even after the millis() overflow, after 49 days
 * 
 * Usage:
 * include "UptimeFormatter.h"
 * Inside your loop() function: 
 * Serial.println("Uptime: " + UptimeFormatter::uptime());
 * 
 * Examples here:
 *
 * Created 08 May 2019
 * By Yiannis Bourkelis
 *
 * https://github.com/YiannisBourkelis/
 */

#include <Arduino.h> // for millis()

#include "Uptime.h"

// Elapsed values before splitting into display units.
unsigned long Uptime::elapsedMilliseconds_;
unsigned long Uptime::elapsedSeconds_;
unsigned long Uptime::elapsedMinutes_;
unsigned long Uptime::elapsedHours_;
unsigned long Uptime::elapsedDays_;

// Display-unit values for the current uptime.
unsigned long Uptime::milliseconds_;
unsigned long Uptime::seconds_;
unsigned long Uptime::minutes_;
unsigned long Uptime::hours_;

// Values retained across a millis() overflow.
unsigned long Uptime::lastMilliseconds_ = 0;
unsigned long Uptime::remainingSeconds_ = 0;
unsigned long Uptime::remainingMinutes_ = 0;
unsigned long Uptime::remainingHours_ = 0;
unsigned long Uptime::remainingDays_ = 0;

// Modified 2026-09-21: renamed the public class to Uptime.
Uptime::Uptime() {
}

/** Return the elapsed milliseconds within the current second. */
unsigned long Uptime::milliseconds() {
  return Uptime::milliseconds_;
}
unsigned long Uptime::seconds() {
  return Uptime::seconds_;
}
unsigned long Uptime::minutes() {
  return Uptime::minutes_;
}
unsigned long Uptime::hours() {
  return Uptime::hours_;
}
unsigned long Uptime::days() {
  return Uptime::elapsedDays_;
}

// Calculate milliseconds, seconds, hours, and days and store them in static values.
void Uptime::calculateUptime() {
  Uptime::elapsedMilliseconds_ = millis();

  if (Uptime::lastMilliseconds_ > Uptime::elapsedMilliseconds_) {
    // In case of millis() overflow, preserve the elapsed display units.
    Uptime::remainingSeconds_ = Uptime::seconds_;
    Uptime::remainingMinutes_ = Uptime::minutes_;
    Uptime::remainingHours_ = Uptime::hours_;
    Uptime::remainingDays_ = Uptime::elapsedDays_;
  }
  // Store millis() to detect an overflow on the next call.
  Uptime::lastMilliseconds_ = Uptime::elapsedMilliseconds_;

  // Convert elapsed milliseconds to total seconds, minutes, hours, and days.
  // Overflow values keep uptime continuous after millis() rolls over.
  Uptime::elapsedSeconds_ = (Uptime::elapsedMilliseconds_ / 1000) + Uptime::remainingSeconds_;
  Uptime::elapsedMinutes_ = (Uptime::elapsedSeconds_ / 60) + Uptime::remainingMinutes_;
  Uptime::elapsedHours_ = (Uptime::elapsedMinutes_ / 60) + Uptime::remainingHours_;
  Uptime::elapsedDays_ = (Uptime::elapsedHours_ / 24) + Uptime::remainingDays_;

  // Split elapsed values into display units. Days are already total days.
  Uptime::milliseconds_ = Uptime::elapsedMilliseconds_ % 1000;
  Uptime::seconds_ = Uptime::elapsedSeconds_ % 60;
  Uptime::minutes_ = Uptime::elapsedMinutes_ % 60;
  Uptime::hours_ = Uptime::elapsedHours_ % 24;
}
