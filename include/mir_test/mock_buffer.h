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
 * Authored by: Thomas Voss <thomas.voss@canonical.com>
 */

#ifndef MIR_COMPOSITOR_MOCK_BUFFER_H_
#define MIR_COMPOSITOR_MOCK_BUFFER_H_

#include "mir/compositor/buffer.h"
#include "mir/geometry/dimensions.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mir
{
namespace compositor
{

namespace geom = mir::geometry;

struct MockIPCPackage : public BufferIPCPackage
{
    MOCK_METHOD0(get_ipc_data, std::vector<int>());
    MOCK_METHOD0(get_ipc_fds , std::vector<int>());
};

struct MockBuffer : public Buffer
{
 public:
    MockBuffer(geom::Width w,
               geom::Height h,
               geom::Stride s,
               PixelFormat pf)
	{
        mock_ipc_package = std::make_shared<MockIPCPackage>();

	    using namespace testing;
        ON_CALL(*this, width())
                .WillByDefault(Return(w));
        ON_CALL(*this, height())
                .WillByDefault(Return(h));
        ON_CALL(*this, stride())
                .WillByDefault(Return(s));
        ON_CALL(*this, pixel_format())
                .WillByDefault(Return(pf));

        ON_CALL(*this, get_ipc_package())
                .WillByDefault(Return(mock_ipc_package));
	}

    MOCK_CONST_METHOD0(width, geom::Width());
    MOCK_CONST_METHOD0(height, geom::Height());
    MOCK_CONST_METHOD0(stride, geom::Stride());
    MOCK_CONST_METHOD0(pixel_format, PixelFormat());
    MOCK_CONST_METHOD0(get_ipc_package, std::shared_ptr<BufferIPCPackage>());

    MOCK_METHOD0(bind_to_texture, void());

    std::shared_ptr<MockIPCPackage> mock_ipc_package;
};

}
}

#endif // MIR_COMPOSITOR_MOCK_BUFFER_H_
