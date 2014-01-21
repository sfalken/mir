/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_GRAPHICS_ANDROID_HWC_FB_DEVICE_H_
#define MIR_GRAPHICS_ANDROID_HWC_FB_DEVICE_H_

#include "hwc_common_device.h"
#include "hwc_layerlist.h"
#include "hardware/gralloc.h"
#include "hardware/fb.h"

namespace mir
{
namespace graphics
{
namespace android
{

class HwcFbDevice : public HWCCommonDevice
{
public:
    HwcFbDevice(std::shared_ptr<hwc_composer_device_1> const& hwc_device,
                std::shared_ptr<framebuffer_device_t> const& fb_device,
                std::shared_ptr<HWCVsyncCoordinator> const& coordinator);

    void prepare_gl();
    void prepare_gl_and_overlays(std::list<std::shared_ptr<Renderable>> const& list); 
    void gpu_render(EGLDisplay dpy, EGLSurface sur);
    void post(Buffer const& buffer);

private:
    std::shared_ptr<framebuffer_device_t> const fb_device;
    bool const use_fb_target;
    static int const num_displays{1};
    LayerList layer_list;
};

}
}
}

#endif /* MIR_GRAPHICS_ANDROID_HWC_FB_DEVICE_H_ */
