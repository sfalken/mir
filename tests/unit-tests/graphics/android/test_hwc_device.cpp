/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "src/server/graphics/android/hwc11_device.h"
#include "mir_test_doubles/mock_hwc_composer_device_1.h"

#include <thread>
#include <chrono>
#include <stdexcept>
#include <memory>
#include <gtest/gtest.h>

namespace mga=mir::graphics::android;
namespace mtd=mir::test::doubles;

class HWCDevice : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        mock_device = std::make_shared<testing::NiceMock<mtd::MockHWCComposerDevice1>>();
    }

    std::shared_ptr<mtd::MockHWCComposerDevice1> mock_device;
};

TEST_F(HWCDevice, test_proc_registration)
{
    using namespace testing;

    hwc_procs_t const* procs;
    EXPECT_CALL(*mock_device, registerProcs_interface(mock_device.get(), _))
        .Times(1)
        .WillOnce(SaveArg<1>(&procs));

    mga::HWC11Device device(mock_device);

    EXPECT_NE(nullptr, procs->invalidate);
    EXPECT_NE(nullptr, procs->vsync);
    EXPECT_NE(nullptr, procs->hotplug);
}

TEST_F(HWCDevice, test_vsync_activation_comes_after_proc_registration)
{
    using namespace testing;

    InSequence sequence_enforcer; 
    EXPECT_CALL(*mock_device, registerProcs_interface(mock_device.get(),_))
        .Times(1);
    EXPECT_CALL(*mock_device, eventControl_interface(mock_device.get(), 0, HWC_EVENT_VSYNC, 1))
        .Times(1)
        .WillOnce(Return(0));

    mga::HWC11Device device(mock_device);
}

TEST_F(HWCDevice, test_vsync_activation_failure_throws)
{
    using namespace testing;

    EXPECT_CALL(*mock_device, eventControl_interface(mock_device.get(), 0, HWC_EVENT_VSYNC, 1))
        .Times(1)
        .WillOnce(Return(-EINVAL));

    EXPECT_THROW({
        mga::HWC11Device device(mock_device);
    }, std::runtime_error);
}


namespace
{
static mga::HWC11Device *global_device;
void* waiting_device(void*)
{
    global_device->wait_for_vsync();
    return NULL;
}
}

TEST_F(HWCDevice, test_vsync_hook_waits)
{
    mga::HWC11Device device(mock_device);
    global_device = &device;

    pthread_t thread;
    pthread_create(&thread, NULL, waiting_device, NULL);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    void* retval;
    auto error = pthread_tryjoin_np(thread, &retval);
    ASSERT_EQ(EBUSY, error);

    device.notify_vsync();
    error = pthread_join(thread, &retval);
    ASSERT_EQ(0, error);

}

TEST_F(HWCDevice, test_vsync_hook_is_callable)
{
    using namespace testing;

    hwc_procs_t const* procs;
    EXPECT_CALL(*mock_device, registerProcs_interface(mock_device.get(), _))
        .Times(1)
        .WillOnce(SaveArg<1>(&procs));

    mga::HWC11Device device(mock_device);
    global_device = &device;

    pthread_t thread;
    pthread_create(&thread, NULL, waiting_device, NULL);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    void* retval;
    auto error = pthread_tryjoin_np(thread, &retval);
    ASSERT_EQ(EBUSY, error);

    procs->vsync(procs, 0, 0);
    error = pthread_join(thread, &retval);
    ASSERT_EQ(0, error);
}

namespace
{

static hwc_display_contents_1_t** saved_displays;

int save_set_arguments(struct hwc_composer_device_1 *, size_t, hwc_display_contents_1_t** displays)
{
    if (nullptr == displays)
        return -1;
    
    saved_displays =    (hwc_display_contents_1_t**) malloc(sizeof(hwc_display_contents_1_t*));
    saved_displays[0] = (hwc_display_contents_1_t*) malloc(sizeof(hwc_display_contents_1_t));


    hwc_display_contents_1_t* primary_display = displays[0];
    memcpy(saved_displays[0], primary_display, sizeof(hwc_display_contents_1_t));

    return 0;
}

void free_save_arguments()
{
    free(saved_displays[0]);
    free(saved_displays);
}

}

TEST_F(HWCDevice, test_hwc_gles_set)
{
    using namespace testing;

    mga::HWC11Device device(mock_device);

    EXPECT_CALL(*mock_device, set_interface(mock_device.get(), HWC_NUM_DISPLAY_TYPES, _))
        .Times(1)
        .WillOnce(Invoke(save_set_arguments));

    device.commit_frame();

    EXPECT_NE(nullptr, *saved_displays);
    EXPECT_EQ(0u,  (*saved_displays)->numHwLayers);
    EXPECT_EQ(-1, (*saved_displays)->retireFenceFd);

    free_save_arguments();
}

TEST_F(HWCDevice, test_hwc_gles_set_error)
{
    using namespace testing;

    mga::HWC11Device device(mock_device);
    EXPECT_CALL(*mock_device, set_interface(mock_device.get(), HWC_NUM_DISPLAY_TYPES, _))
        .Times(1)
        .WillOnce(Return(-1));

    EXPECT_THROW({
        device.commit_frame();
    }, std::runtime_error);
}
