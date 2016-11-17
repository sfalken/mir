/*
 * Copyright © 2016 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#ifndef MIR_THROTTLE_H_
#define MIR_THROTTLE_H_

#include "mir/time/posix_timestamp.h"
#include <chrono>
#include <functional>

namespace mir {

class Throttle
{
public:
    typedef std::function<time::PosixTimestamp()> ResyncCallback;
    typedef std::function<time::PosixTimestamp(clockid_t)> GetCurrentTime;

    Throttle(GetCurrentTime);

    /**
     * Set the precise frame period in nanoseconds (1000000000/Hz).
     */
    void set_period(std::chrono::nanoseconds);

    /**
     * Set the frame frequency in Hertz.
     * This is just a convenient wrapper around set_period, although slightly
     * less precise.
     */
    void set_frequency(double hz);

    /**
     * Optionally set a callback that queries the server to ask for the
     * latest hardware vsync timestamp. This provides phase correction for
     * increased precision but is not strictly required.
     */
    void set_resync_callback(ResyncCallback);

    /**
     * Return the next timestamp to sleep_until, which comes after the last one
     * that was slept till. On the first frame you can just provide an
     * uninitialized timestamp.
     */
    time::PosixTimestamp next_frame_after(time::PosixTimestamp prev) const;

private:
    time::PosixTimestamp fake_resync_callback() const;

    GetCurrentTime const get_current_time;
    mutable bool readjustment_required;
    std::chrono::nanoseconds period;
    ResyncCallback resync_callback;
};

} // namespace mir

#endif // MIR_THROTTLE_H_
