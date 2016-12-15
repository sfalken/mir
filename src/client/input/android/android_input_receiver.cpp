/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "android_input_receiver.h"

#include "mir/input/xkb_mapper.h"
#include "mir/input/input_receiver_report.h"
#include "mir/input/android/android_input_lexicon.h"
#include "mir/events/event_private.h"

#include <boost/throw_exception.hpp>

#include <androidfw/InputTransport.h>

#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <system_error>
#include <cstdlib>

namespace mircv = mir::input::receiver;
namespace mircva = mircv::android;
namespace md = mir::dispatch;

namespace mia = mir::input::android;

namespace
{
int valid_fd_or_system_error(int fd, char const* message)
{
    if (fd < 0)
        BOOST_THROW_EXCEPTION((std::system_error{errno,
                                                 std::system_category(),
                                                 message}));
    return fd;
}
}

mircva::InputReceiver::InputReceiver(droidinput::sp<droidinput::InputChannel> const& input_channel,
                                     std::shared_ptr<mircv::XKBMapper> const& keymapper,
                                     std::function<void(MirEvent*)> const& event_handling_callback,
                                     std::shared_ptr<mircv::InputReceiverReport> const& report,
                                     AndroidClock clock)
  : timer_fd{valid_fd_or_system_error(timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC),
        "Failed to create IO timer")},
    wake_fd{valid_fd_or_system_error(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE),
        "Failed to create IO wakeup notifier")},
    input_channel(input_channel),
    handler{event_handling_callback},
    xkb_mapper(keymapper),
    report(report),
    input_consumer(std::make_shared<droidinput::InputConsumer>(input_channel)),
    android_clock(clock)
{
    /*
     * 59Hz by default. This ensures the input rate never gets ahead of the
     * typical display rate, which would be seen as visible lag.
     *
     * TODO: Although we could now replace this with the real refresh rate of
     *       the display (which MirSurface knows), it would be better to
     *       use real display timestamps (when we have them) for even greater
     *       precision. And better again to delete all the input resampling
     *       logic since client-side vsync now means Qt can safely remove
     *       the QML_NO_TOUCH_COMPRESSION=1 workaround and use its own
     *       resampling. Because client-side vsync should make the Qt
     *       algorithm safe to use with Mir nesting.
     */
    event_rate_hz = 59;
    auto env = getenv("MIR_CLIENT_INPUT_RATE");
    if (env != NULL)
        event_rate_hz = atoi(env);

    dispatcher.add_watch(timer_fd, [this]()
    {
        consume_wake_notification(timer_fd);
        process_and_maybe_send_event();
    });

    dispatcher.add_watch(wake_fd, [this]()
    {
        consume_wake_notification(wake_fd);
        process_and_maybe_send_event();
    });

    dispatcher.add_watch(mir::Fd{mir::IntOwnedFd{input_channel->getFd()}},
                         [this]() { process_and_maybe_send_event(); });
}

mircva::InputReceiver::InputReceiver(int fd,
                                     std::shared_ptr<mircv::XKBMapper> const& keymapper,
                                     std::function<void(MirEvent*)> const& event_handling_callback,
                                     std::shared_ptr<mircv::InputReceiverReport> const& report,
                                     AndroidClock clock)
    : InputReceiver(new droidinput::InputChannel(droidinput::String8(""), fd),
                    keymapper,
                    event_handling_callback,
                    report,
                    clock)
{
}

mircva::InputReceiver::~InputReceiver()
{
}

mir::Fd mircva::InputReceiver::watch_fd() const
{
    return dispatcher.watch_fd();
}

bool mircva::InputReceiver::dispatch(md::FdEvents events)
{
    return dispatcher.dispatch(events);
}

md::FdEvents mircva::InputReceiver::relevant_events() const
{
    return dispatcher.relevant_events();
}

namespace
{

static void map_key_event(std::shared_ptr<mircv::XKBMapper> const& xkb_mapper, MirEvent &ev)
{
    // TODO: As XKBMapper is used to track modifier state we need to use a seperate instance
    // of XKBMapper per device id (or modify XKBMapper semantics)
    if (mir_event_get_type(&ev) != mir_event_type_input)
        return;

    xkb_mapper->map_event(ev);
}

}

void mircva::InputReceiver::process_and_maybe_send_event()
{
    droidinput::InputEvent *android_event;
    uint32_t event_sequence_id;

    /*
     * Enable "Project Butter" input resampling in InputConsumer::consume():
     *   consumeBatches = true, so as to ensure the "cooked" event rate that
     *      clients experience is at least the minimum of event_rate_hz
     *      and the raw device event rate.
     *   frame_time = A regular interval. This provides a virtual frame
     *      interval during which InputConsumer will collect raw events,
     *      resample them and emit a "cooked" event back to us at roughly every
     *      60th of a second. "cooked" events are both smoothed and
     *      extrapolated/predicted into the future (for tool=finger) giving the
     *      appearance of lower latency. Getting a real frame time from the
     *      graphics logic (which is messy) does not appear to be necessary to
     *      gain significant benefit.
     */

    auto frame_time = std::chrono::nanoseconds(-1);

    if (event_rate_hz > 0)
    {
        auto one_frame = std::chrono::nanoseconds(1000000000ULL / event_rate_hz);
        std::chrono::nanoseconds const
            now = android_clock(SYSTEM_TIME_MONOTONIC);
        frame_time = (now / one_frame) * one_frame;
    }

    auto result = input_consumer->consume(&event_factory,
                                          true,
                                          frame_time,
                                          &event_sequence_id,
                                          &android_event);
    if (result == droidinput::OK)
    {
        auto ev = mia::Lexicon::translate(android_event);
        map_key_event(xkb_mapper, *ev);
        report->received_event(*ev);
        handler(ev.get());

        // TODO: It would be handy in future if handler returned a bool
        //       indicating whether the event was used so that if not it might
        //       get passed on to someone else - passed into here:
        input_consumer->sendFinishedSignal(event_sequence_id, true);
    }
    if (input_consumer->hasDeferredEvent())
    {
        // input_consumer->consume() can read an event from the fd and find that the event cannot
        // be added to the current batch.
        //
        // In this case, it emits the current batch and leaves the new event pending.
        // This means we have an event we need to dispatch, but as it has already been read from
        // the fd we cannot rely on being woken by the fd being readable.
        //
        // So, we ensure we'll appear dispatchable by pushing an event to the wakeup pipe.
        wake();
    }
    else if (input_consumer->hasPendingBatch() && event_rate_hz > 0)
    {
        using namespace std::chrono;
        // If we batch according to a fixed event rate - we wait until the next "frame" occurs.
        auto one_frame = nanoseconds(1000000000ULL / event_rate_hz);
        auto now = android_clock(SYSTEM_TIME_MONOTONIC);
        auto next_frame = frame_time + one_frame;

        if (next_frame <= now)
        {
            wake();
        }
        else
        {
            auto full_sec = duration_cast<duration<long>>(next_frame);
            auto nano_sec = duration_cast<duration<long,std::nano>>(next_frame - full_sec);
            struct itimerspec const frame_timeout = {
                { 0, 0 },
                { full_sec.count(), nano_sec.count()}
            };
            if (timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &frame_timeout, NULL) < 0)
            {
                BOOST_THROW_EXCEPTION((std::system_error{errno,
                                       std::system_category(),
                                       "Failed to arm timer"}));
            }
        }
    }
}

void mircva::InputReceiver::consume_wake_notification(mir::Fd const& fd)
{
    uint64_t dummy;
    if (read(fd, &dummy, sizeof(dummy)) != sizeof(dummy))
    {
        BOOST_THROW_EXCEPTION((std::system_error{errno,
                                                 std::system_category(),
                                                 "Failed to consume notification"}));
    }
}

void mircva::InputReceiver::wake()
{
    uint64_t one{1};
    if (write(wake_fd, &one, sizeof(one)) != sizeof(one))
    {
        BOOST_THROW_EXCEPTION((std::system_error{errno,
                                                 std::system_category(),
                                                 "Failed to notify IO loop"}));
    }
}
