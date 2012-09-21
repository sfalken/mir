/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_MIR_TEST_GRAPHICS_GBM_MOCK_GBM_H_
#define MIR_MIR_TEST_GRAPHICS_GBM_MOCK_GBM_H_

#include <gmock/gmock.h>

#include <gbm.h>

namespace mir
{
namespace graphics
{
namespace gbm
{

class FakeGBMResources
{
public:
    FakeGBMResources();
    ~FakeGBMResources() = default;

    gbm_device  *device;
    gbm_surface *surface;
    gbm_bo *bo;
    gbm_bo_handle bo_handle;
};

class MockGBM
{
public:
    MockGBM();
    ~MockGBM();

    MOCK_METHOD1(gbm_create_device, struct gbm_device*(int fd));
    MOCK_METHOD1(gbm_device_destroy, void(struct gbm_device *gbm));
    MOCK_METHOD1(gbm_device_get_fd, int(struct gbm_device *gbm));

    MOCK_METHOD5(gbm_surface_create, struct gbm_surface*(struct gbm_device *gbm,
                                                         uint32_t width, uint32_t height,
                                                         uint32_t format, uint32_t flags));
    MOCK_METHOD1(gbm_surface_destroy, void(struct gbm_surface *surface));
    MOCK_METHOD1(gbm_surface_lock_front_buffer, struct gbm_bo*(struct gbm_surface *surface));
    MOCK_METHOD2(gbm_surface_release_buffer, void(struct gbm_surface *surface, struct gbm_bo *bo));

    MOCK_METHOD5(gbm_bo_create, struct gbm_bo*(struct gbm_device *gbm,
                                               uint32_t width, uint32_t height,
                                               uint32_t format, uint32_t flags));
    MOCK_METHOD1(gbm_bo_get_device, struct gbm_device*(struct gbm_bo *bo));
    MOCK_METHOD1(gbm_bo_get_width, uint32_t(struct gbm_bo *bo));
    MOCK_METHOD1(gbm_bo_get_height, uint32_t(struct gbm_bo *bo));
    MOCK_METHOD1(gbm_bo_get_stride, uint32_t(struct gbm_bo *bo));
    MOCK_METHOD1(gbm_bo_get_format, uint32_t(struct gbm_bo *bo));
    MOCK_METHOD1(gbm_bo_get_handle, union gbm_bo_handle(struct gbm_bo *bo));
    MOCK_METHOD3(gbm_bo_set_user_data, void(struct gbm_bo *bo, void *data,
                                            void (*destroy_user_data)(struct gbm_bo *, void *)));
    MOCK_METHOD1(gbm_bo_get_user_data, void*(struct gbm_bo *bo));
    MOCK_METHOD1(gbm_bo_destroy, void(struct gbm_bo *bo));

    FakeGBMResources fake_gbm;

private:
    void on_gbm_bo_set_user_data(struct gbm_bo *bo, void *data,
                                            void (*destroy_user_data)(struct gbm_bo *, void *))
    {
        destroyers.push_back(Destroyer{bo, data, destroy_user_data});
    }

    struct Destroyer
    {
        struct gbm_bo *bo;
        void *data;
        void (*destroy_user_data)(struct gbm_bo *, void *);

        void operator()() const { destroy_user_data(bo, data); }
    };

    std::vector<Destroyer> destroyers;
};

}
}
}

#endif /* MIR_MIR_TEST_GRAPHICS_GBM_MOCK_GBM_H_ */
