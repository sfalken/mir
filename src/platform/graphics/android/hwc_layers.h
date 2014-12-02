/*
 * Copyright © 2014 Canonical Ltd.
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

#ifndef MIR_GRAPHICS_ANDROID_HWC_LAYERS_H_
#define MIR_GRAPHICS_ANDROID_HWC_LAYERS_H_

#include "mir/graphics/renderable.h"
#include "mir/graphics/android/fence.h"
#include "mir/geometry/rectangle.h"
#include <hardware/hwcomposer.h>
#include <memory>
#include <vector>
#include <initializer_list>
#include <list>

namespace mir
{
namespace graphics
{

class Renderable;
class Buffer;
class NativeBuffer;

namespace android
{
enum LayerType
{
    gl_rendered,
    overlay,
    framebuffer_target,
    skip
};

class LayerSourceCrop
{
public:
    virtual void fill_source_crop(hwc_layer_1_t&, geometry::Rectangle const& crop_size) const = 0;
    virtual ~LayerSourceCrop() = default;
    LayerSourceCrop() = default;
    LayerSourceCrop(LayerSourceCrop const&) = delete; 
    LayerSourceCrop& operator=(LayerSourceCrop const&) = delete; 
};

//HWC 1.0 to 1.2 have int sourceCrop
class IntegerSourceCrop : public LayerSourceCrop
{
    void fill_source_crop(hwc_layer_1_t&, geometry::Rectangle const& crop_size) const override;
};

//HWC 1.3 and later have float sourceCrop
class FloatSourceCrop : public LayerSourceCrop
{
    void fill_source_crop(hwc_layer_1_t&, geometry::Rectangle const& crop_size) const override;
};

class HWCLayer
{
public:
    HWCLayer(
        std::shared_ptr<LayerSourceCrop> const&,
        std::shared_ptr<hwc_display_contents_1_t> const& list,
        size_t layer_index);

    HWCLayer(
        std::shared_ptr<LayerSourceCrop> const&,
        std::shared_ptr<hwc_display_contents_1_t> const& list,
        size_t layer_index,
        LayerType,
        geometry::Rectangle const& screen_position,
        bool alpha_enabled,
        Buffer const& buffer);

    HWCLayer& operator=(HWCLayer && layer);
    HWCLayer(HWCLayer && layer);

    HWCLayer& operator=(HWCLayer const& layer) = delete;
    HWCLayer(HWCLayer const& layer) = delete;
    
    bool setup_layer(
        LayerType type,
        geometry::Rectangle const& position,
        bool alpha_enabled,
        Buffer const& buffer);
    bool needs_gl_render() const;
    void set_acquirefence_from(Buffer const& buffer);
    void update_from_releasefence(Buffer const& buffer);
private:
    std::shared_ptr<LayerSourceCrop> source_crop;
    hwc_layer_1_t* hwc_layer;
    std::shared_ptr<hwc_display_contents_1_t> hwc_list;
    hwc_rect_t visible_rect;
};
}
}
}

#endif /* MIR_GRAPHICS_ANDROID_HWC_LAYERS_H_ */
