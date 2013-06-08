/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2012 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#ifndef XCSOAR_ANDROID_NOOK_HPP
#define XCSOAR_ANDROID_NOOK_HPP

#include "Language/Language.hpp"

namespace Nook {

  /**
   * controls the Nook's charge level so it gently charges or
   * discharges the battery.
   * Does this by keeping battery level within a range around the
   * battery percent on startup
   * The difficulty is that if we set the charge rate too high,
   * the Nook's USB and IOIO connections appear less reliable
   */
  class BatteryController
  {
    /**
     * the rate we told the Nook we want to charge (mA)
     */
    unsigned last_charge_rate;

    /**
     * upper limit of range where we want to maintain the battery level
     */
    unsigned upper_battery_threshhold;

    bool initialised;

  public:
    BatteryController()
      :last_charge_rate(0) {
    }

  public:
    void ProcessChargeRate(unsigned value);
    /**
     * true if we told the Nook to Charge at 500mA or higher
     */

    void Initialise(unsigned value);

    bool IsInitialised() {
      return initialised;
    }

    bool IsCharging() {
      return last_charge_rate > 100;
    }

  protected:
    unsigned GetUpperChargeThreshhold() {
      return upper_battery_threshhold;
    }

    unsigned GetLowerChargeThreshhold() {
      return upper_battery_threshhold - 2;
    }

    void SetCharging();

    void SetDischarging();
  };

  /**
   * initialize USB mode in Nook (must be rooted with USB Kernel)
   */
  void InitUsb();

  /**
   * Enter FastMode to eliminate full refresh of screen
   * requires Nook kernel rooted to support FastMode
   */
  void EnterFastMode();

  /**
   * Exit FastMode to restore full (slow) refresh of screen
   * requires Nook kernel rooted to support FastMode
   */
  void ExitFastMode();

  /**
   * Set charge rate to 500mA.
   */
  void SetCharge500();

  /**
   * Set charge rate to 100mA.
   */
  void SetCharge100();

#define GetNookUsbHostDriverName() "Nook ST USB Host"

  const char* GetUsbHostDriverHelp();
  const char* GetUsbHostDriverPath();

}

#endif
