/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_CLIENT_ANDROID_MIR_NATIVE_WINDOW_H_
#define MIR_CLIENT_ANDROID_MIR_NATIVE_WINDOW_H_

#include "../mir_client_surface.h"
#include <system/window.h>
#include <cstdarg>
#include <memory>

namespace mir
{
namespace client
{
namespace android
{
class AndroidDriverInterpreter;

class MirNativeWindow : public ANativeWindow
{
public:
    explicit MirNativeWindow(std::shared_ptr<AndroidDriverInterpreter> interpreter);

    int query_internal(int key, int* value) const;
    int perform_internal(int key, va_list args );
    int dequeueBuffer_internal(struct ANativeWindowBuffer** buffer);
    int queueBuffer_internal(struct ANativeWindowBuffer* buffer, int fence_fd);
private:

    std::shared_ptr<AndroidDriverInterpreter> driver_interpreter;
};

}
}
}

#endif /* MIR_CLIENT_ANDROID_MIR_NATIVE_WINDOW_H_ */
