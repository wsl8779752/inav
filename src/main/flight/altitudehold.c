/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

#include "common/maths.h"
#include "common/axis.h"

#include "drivers/accgyro.h"
#include "drivers/serial.h"

// FIXME remove dependency on currentProfile and masterConfig globals and clean up include file list.

#include "flight/flight.h"
#include "sensors/sensors.h"
#include "sensors/acceleration.h"
#include "sensors/barometer.h"
#include "sensors/battery.h"
#include "sensors/boardalignment.h"
#include "sensors/gyro.h"

#include "io/escservo.h"
#include "io/gimbal.h"
#include "io/gps.h"
#include "io/serial.h"
#include "flight/failsafe.h"
#include "flight/imu.h"
#include "flight/mixer.h"
#include "telemetry/telemetry.h"

#include "rx/rx.h"
#include "io/rc_controls.h"

#include "config/runtime_config.h"
#include "config/config.h"
#include "config/config_profile.h"
#include "config/config_master.h"

#ifdef BARO
static int16_t initialThrottleHold;

static void multirotorAltHold(void)
{
    static uint8_t isAltHoldChanged = 0;
    static int16_t AltHoldCorr = 0;
    // multirotor alt hold
    if (currentProfile.alt_hold_fast_change) {
        // rapid alt changes
        if (abs(rcCommand[THROTTLE] - initialThrottleHold) > currentProfile.alt_hold_deadband) {
            errorAltitudeI = 0;
            isAltHoldChanged = 1;
            rcCommand[THROTTLE] += (rcCommand[THROTTLE] > initialThrottleHold) ? -currentProfile.alt_hold_deadband : currentProfile.alt_hold_deadband;
        } else {
            if (isAltHoldChanged) {
                AltHold = EstAlt;
                isAltHoldChanged = 0;
            }
            rcCommand[THROTTLE] = constrain(initialThrottleHold + BaroPID, masterConfig.escAndServoConfig.minthrottle + 100, masterConfig.escAndServoConfig.maxthrottle);
        }
    } else {
        // slow alt changes for apfags
        if (abs(rcCommand[THROTTLE] - initialThrottleHold) > currentProfile.alt_hold_deadband) {
            // Slowly increase/decrease AltHold proportional to stick movement ( +100 throttle gives ~ +50 cm in 1 second with cycle time about 3-4ms)
            AltHoldCorr += rcCommand[THROTTLE] - initialThrottleHold;
            AltHold += AltHoldCorr / 2000;
            AltHoldCorr %= 2000;
            isAltHoldChanged = 1;
        } else if (isAltHoldChanged) {
            AltHold = EstAlt;
            AltHoldCorr = 0;
            isAltHoldChanged = 0;
        }
        rcCommand[THROTTLE] = constrain(initialThrottleHold + BaroPID, masterConfig.escAndServoConfig.minthrottle + 100, masterConfig.escAndServoConfig.maxthrottle);
    }
}

static void fixedWingAltHold()
{
    // handle fixedwing-related althold. UNTESTED! and probably wrong
    // most likely need to check changes on pitch channel and 'reset' althold similar to
    // how throttle does it on multirotor

    rcCommand[PITCH] += BaroPID * masterConfig.fixedwing_althold_dir;
}

void updateAltHold(void)
{
    if (f.FIXED_WING) {
        fixedWingAltHold();
    } else {
        multirotorAltHold();
    }
}

void updateAltHoldState(void)
{
    // Baro alt hold activate
    if (rcOptions[BOXBARO]) {
        if (!f.BARO_MODE) {
            f.BARO_MODE = 1;
            AltHold = EstAlt;
            initialThrottleHold = rcCommand[THROTTLE];
            errorAltitudeI = 0;
            BaroPID = 0;
        }
    } else {
        f.BARO_MODE = 0;
    }
}

#endif
