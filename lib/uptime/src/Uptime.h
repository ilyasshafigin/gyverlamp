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
 * Uptime library for Arduino devices
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
 * 
 *Complete documentation for each function and variable name exist
 *inside the implementation Uptime.cpp file
 */

// Modified 2026-09-21: renamed the public class to Uptime.
class Uptime {
public:
  Uptime();

  static void calculateUptime();

  static unsigned long milliseconds();
  static unsigned long seconds();
  static unsigned long minutes();
  static unsigned long hours();
  static unsigned long days();

private:
  static unsigned long elapsedMilliseconds_;
  static unsigned long elapsedSeconds_;
  static unsigned long elapsedMinutes_;
  static unsigned long elapsedHours_;
  static unsigned long elapsedDays_;

  static unsigned long milliseconds_;
  static unsigned long seconds_;
  static unsigned long minutes_;
  static unsigned long hours_;

  static unsigned long lastMilliseconds_;
  static unsigned long remainingSeconds_;
  static unsigned long remainingMinutes_;
  static unsigned long remainingHours_;
  static unsigned long remainingDays_;
};
