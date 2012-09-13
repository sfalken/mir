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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir/graphics/android/android_buffer.h"
#include "mir_test/mock_alloc_adaptor.h"

#include <hardware/gralloc.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>

namespace mc=mir::compositor;
namespace mg=mir::graphics;
namespace mga=mir::graphics::android;
namespace geom=mir::geometry;

class AndroidGraphicBufferBasic : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        mock_buffer_handle = std::make_shared<mga::MockBufferHandle>();
        mock_alloc_device = std::make_shared<mga::MockAllocAdaptor>(mock_buffer_handle);

        /* set up common defaults */
        pf = mc::PixelFormat::rgba_8888;
        width = geom::Width(300);
        height = geom::Height(200);

    }

    native_handle_t native_handle;
    std::shared_ptr<mga::MockAllocAdaptor> mock_alloc_device;
    std::shared_ptr<mga::MockBufferHandle> mock_buffer_handle;
    mc::PixelFormat pf;
    geom::Width width;
    geom::Height height;
};


TEST_F(AndroidGraphicBufferBasic, basic_allocation_is_non_null)
{
    using namespace testing;

    EXPECT_CALL(*mock_alloc_device, alloc_buffer( _, _, _, _));

    std::shared_ptr<mc::Buffer> buffer(new mga::AndroidBuffer(mock_alloc_device, width, height, pf));

    EXPECT_NE((int)buffer.get(), NULL);
}

TEST_F(AndroidGraphicBufferBasic, usage_type_is_set_to_hardware_by_default)
{
    using namespace testing;

    EXPECT_CALL(*mock_alloc_device, alloc_buffer( _, _, _, mga::BufferUsage::use_hardware));

    std::shared_ptr<mc::Buffer> buffer(new mga::AndroidBuffer(mock_alloc_device, width, height, pf));
}

TEST_F(AndroidGraphicBufferBasic, width_query_test)
{
    using namespace testing;

    geom::Width test_width(443);

    EXPECT_CALL(*mock_buffer_handle, width())
    .Times(Exactly(1))
    .WillOnce(Return(test_width));
    std::shared_ptr<mc::Buffer> buffer(new mga::AndroidBuffer(mock_alloc_device, width, height, pf));

    EXPECT_EQ(test_width, buffer->width());
}

TEST_F(AndroidGraphicBufferBasic, height_query_test)
{
    using namespace testing;

    geom::Height test_height(431);

    EXPECT_CALL(*mock_buffer_handle, height())
    .Times(Exactly(1))
    .WillOnce(Return(test_height));
    EXPECT_CALL(*mock_alloc_device, alloc_buffer( _, height, _, _ ));
    std::shared_ptr<mc::Buffer> buffer(new mga::AndroidBuffer(mock_alloc_device, width, height, pf));

    EXPECT_EQ(test_height, buffer->height());
}

TEST_F(AndroidGraphicBufferBasic, format_passthrough_test)
{
    using namespace testing;

    EXPECT_CALL(*mock_alloc_device, alloc_buffer( _, _, pf, _ ));
    std::shared_ptr<mc::Buffer> buffer(new mga::AndroidBuffer(mock_alloc_device, width, height, pf));

}

TEST_F(AndroidGraphicBufferBasic, format_queries_handle_test)
{
    using namespace testing;

    mc::PixelFormat pf2 = mc::PixelFormat::rgba_5658;

    EXPECT_CALL(*mock_buffer_handle, format())
    .Times(Exactly(1))
    .WillOnce(Return(pf2));
    EXPECT_CALL(*mock_alloc_device, alloc_buffer( _, _ , _, _ ));
    std::shared_ptr<mc::Buffer> buffer(new mga::AndroidBuffer(mock_alloc_device, width, height, pf));

    EXPECT_EQ(buffer->pixel_format(), pf2);

}

struct EmptyDeleter
{
    template<typename T>
    void operator()(T* )
    {
    }
};
TEST_F(AndroidGraphicBufferBasic, queries_native_window_for_ipc_ptr)
{
    using namespace testing;

    EmptyDeleter del;
    mc::BufferIPCPackage *package = NULL;
    auto ipc_package = std::shared_ptr<mc::BufferIPCPackage> (package, del);
 
    EXPECT_CALL(*mock_buffer_handle, get_ipc_package())
        .Times(Exactly(1))
        .WillOnce(Return(ipc_package));

    std::shared_ptr<mc::Buffer> buffer(new mga::AndroidBuffer(mock_alloc_device, width, height, pf));
    buffer->get_ipc_package();
}

