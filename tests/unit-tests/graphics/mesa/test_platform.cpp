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

#include "mir/graphics/platform_ipc_package.h"
#include "mir/graphics/drm_authenticator.h"
#include "mir/graphics/event_handler_register.h"
#include "mir/graphics/buffer_ipc_packer.h"
#include "src/platform/graphics/mesa/platform.h"
#include "src/platform/graphics/mesa/internal_client.h"
#include "src/server/report/null_report_factory.h"
#include "mir/emergency_cleanup_registry.h"

#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/mock_buffer_ipc_message.h"
#include "mir_test_doubles/platform_factory.h"
#include "mir_test_doubles/mock_virtual_terminal.h"
#include "mir_test_doubles/null_virtual_terminal.h"

#include <gtest/gtest.h>

#include "mir_test_framework/udev_environment.h"
#include "mir_test/pipe.h"

#include "mir_test_doubles/mock_drm.h"
#include "mir_test_doubles/mock_gbm.h"
#include "mir_test_doubles/fd_matcher.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stdexcept>
#include <atomic>
#include <thread>
#include <chrono>

namespace mg = mir::graphics;
namespace mgm = mir::graphics::mesa;
namespace mtd = mir::test::doubles;
namespace mtf = mir_test_framework;

namespace
{

class MesaGraphicsPlatform : public ::testing::Test
{
public:
    void SetUp()
    {
        ::testing::Mock::VerifyAndClearExpectations(&mock_drm);
        ::testing::Mock::VerifyAndClearExpectations(&mock_gbm);
        fake_devices.add_standard_device("standard-drm-devices");
    }

    std::shared_ptr<mg::Platform> create_platform()
    {
        return mtd::create_platform_with_null_dependencies();
    }

    ::testing::NiceMock<mtd::MockDRM> mock_drm;
    ::testing::NiceMock<mtd::MockGBM> mock_gbm;
    mtf::UdevEnvironment fake_devices;
};
}

TEST_F(MesaGraphicsPlatform, get_ipc_package)
{
    using namespace testing;
    mir::test::Pipe auth_pipe;
    int const auth_fd{auth_pipe.read_fd()};

    /* First time for master DRM fd, second for authenticated fd */
    EXPECT_CALL(mock_drm, open(_,_,_))
        .WillOnce(Return(mock_drm.fake_drm.fd()));
    EXPECT_CALL(mock_drm, drmOpen(_,_))
        .WillOnce(Return(auth_fd));

    /* Expect proper authorization */
    EXPECT_CALL(mock_drm, drmGetMagic(auth_fd,_));
    EXPECT_CALL(mock_drm, drmAuthMagic(mock_drm.fake_drm.fd(),_));

    EXPECT_CALL(mock_drm, drmClose(mock_drm.fake_drm.fd()));

    /* Expect authenticated fd to be closed when package is destroyed */
    EXPECT_CALL(mock_drm, drmClose(auth_fd));

    EXPECT_NO_THROW (
        auto platform = create_platform();
        auto pkg = platform->get_ipc_package();

        ASSERT_TRUE(pkg.get());
        ASSERT_EQ(std::vector<int32_t>::size_type{1}, pkg->ipc_fds.size());
        ASSERT_EQ(auth_fd, pkg->ipc_fds[0]);
    );
}

TEST_F(MesaGraphicsPlatform, a_failure_while_creating_a_platform_results_in_an_error)
{
    using namespace ::testing;

    EXPECT_CALL(mock_drm, open(_,_,_))
            .WillRepeatedly(Return(-1));

    try
    {
        auto platform = create_platform();
    } catch(...)
    {
        return;
    }

    FAIL() << "Expected an exception to be thrown.";
}

TEST_F(MesaGraphicsPlatform, fails_if_no_resources)
{
    using namespace ::testing;

    EXPECT_CALL(mock_drm, drmModeGetResources(_))
        .Times(Exactly(1))
        .WillOnce(Return(reinterpret_cast<drmModeRes*>(0)));

    EXPECT_CALL(mock_drm, drmModeFreeResources(_))
        .Times(Exactly(0));

    EXPECT_THROW({
        auto platform = create_platform();
    }, std::runtime_error) << "Expected that c'tor of Platform throws";
}

/* ipc packaging tests */
TEST_F(MesaGraphicsPlatform, test_ipc_data_packed_correctly)
{
    using namespace testing;
    mtd::MockBuffer mock_buffer;
    mir::geometry::Stride dummy_stride(4390);

    auto native_handle = std::make_shared<MirBufferPackage>();
    native_handle->data_items = 4;
    native_handle->fd_items = 2;
    for(auto i=0; i<mir_buffer_package_max; i++)
    {
        native_handle->fd[i] = i;
        native_handle->data[i] = i;
    }

    EXPECT_CALL(mock_buffer, native_buffer_handle())
        .WillOnce(testing::Return(native_handle));
    EXPECT_CALL(mock_buffer, stride())
        .WillOnce(testing::Return(mir::geometry::Stride{dummy_stride}));
    EXPECT_CALL(mock_buffer, size())
        .WillOnce(testing::Return(mir::geometry::Size{123, 456}));

    auto platform = create_platform();

    mtd::MockPacker mock_packer;
    for(auto i=0; i < native_handle->fd_items; i++)
    {
        EXPECT_CALL(mock_packer, pack_fd(mtd::RawFdMatcher(native_handle->fd[i])))
            .Times(Exactly(1));
    }
    for(auto i=0; i < native_handle->data_items; i++)
    {
        EXPECT_CALL(mock_packer, pack_data(native_handle->data[i]))
            .Times(Exactly(1));
    }
    EXPECT_CALL(mock_packer, pack_stride(dummy_stride))
        .Times(Exactly(1));
    EXPECT_CALL(mock_packer, pack_flags(testing::_))
        .Times(Exactly(1));
    EXPECT_CALL(mock_packer, pack_size(testing::_))
        .Times(Exactly(1));

    auto packer = platform->create_buffer_packer();
    packer->pack_buffer(mock_packer, mock_buffer, mg::BufferIpcMsgType::full_msg);
    packer->pack_buffer(mock_packer, mock_buffer, mg::BufferIpcMsgType::update_msg);
}

TEST_F(MesaGraphicsPlatform, drm_auth_magic_calls_drm_function_correctly)
{
    using namespace testing;

    unsigned int const magic{0x10111213};

    EXPECT_CALL(mock_drm, drmAuthMagic(mock_drm.fake_drm.fd(),magic))
        .WillOnce(Return(0));

    auto platform = create_platform();
    auto authenticator = std::dynamic_pointer_cast<mg::DRMAuthenticator>(platform);
    authenticator->drm_auth_magic(magic);
}

TEST_F(MesaGraphicsPlatform, drm_auth_magic_throws_if_drm_function_fails)
{
    using namespace testing;

    unsigned int const magic{0x10111213};

    EXPECT_CALL(mock_drm, drmAuthMagic(mock_drm.fake_drm.fd(),magic))
        .WillOnce(Return(-1));

    auto platform = create_platform();
    auto authenticator = std::dynamic_pointer_cast<mg::DRMAuthenticator>(platform);

    EXPECT_THROW({
        authenticator->drm_auth_magic(magic);
    }, std::runtime_error);
}

TEST_F(MesaGraphicsPlatform, platform_provides_validation_of_display_for_internal_clients)
{
    MirMesaEGLNativeDisplay* native_display = nullptr;
    EXPECT_EQ(MIR_MESA_FALSE, mgm::mir_server_mesa_egl_native_display_is_valid(native_display));
    {
        auto platform = create_platform();
        auto client = platform->create_internal_client();
        native_display = reinterpret_cast<MirMesaEGLNativeDisplay*>(client->egl_native_display());
        EXPECT_EQ(MIR_MESA_TRUE, mgm::mir_server_mesa_egl_native_display_is_valid(native_display));
    }
    EXPECT_EQ(MIR_MESA_FALSE, mgm::mir_server_mesa_egl_native_display_is_valid(native_display));
}

TEST_F(MesaGraphicsPlatform, egl_native_display_is_gbm_device)
{
    auto platform = create_platform();
    EXPECT_EQ(mock_gbm.fake_gbm.device, platform->egl_native_display());
}

namespace
{

class ConcurrentCallDetector
{
public:
    ConcurrentCallDetector()
        : threads_in_call{0}, detected_concurrent_calls_{false}
    {
    }

    void call()
    {
        if (threads_in_call.fetch_add(1) > 0)
            detected_concurrent_calls_ = true;

        std::this_thread::sleep_for(std::chrono::milliseconds{1});

        --threads_in_call;
    }

    bool detected_concurrent_calls()
    {
        return detected_concurrent_calls_;
    }

private:
    std::atomic<int> threads_in_call;
    std::atomic<bool> detected_concurrent_calls_;
};

}

/*
 * This test is not 100% reliable in theory (we are trying to recreate a race
 * condition after all!), but it can only produce false successes, not false
 * failures, so it's safe to use.  In practice it is reliable enough: I get a
 * 100% failure rate for this test (1000 out of 1000 repetitions) when testing
 * without the fix for the race condition we are testing for.
 */
TEST_F(MesaGraphicsPlatform, drm_close_not_called_concurrently_on_ipc_package_destruction)
{
    using namespace testing;

    unsigned int const num_threads{10};
    unsigned int const num_iterations{10};

    ConcurrentCallDetector detector;

    ON_CALL(mock_drm, drmClose(_))
        .WillByDefault(DoAll(InvokeWithoutArgs(&detector, &ConcurrentCallDetector::call),
                             Return(0)));

    auto platform = create_platform();

    std::vector<std::thread> threads;

    for (unsigned int i = 0; i < num_threads; i++)
    {
        threads.push_back(std::thread{
            [platform]
            {
                for (unsigned int i = 0; i < num_iterations; i++)
                {
                    platform->get_ipc_package();
                }
            }});
    }

    for (auto& t : threads)
        t.join();

    EXPECT_FALSE(detector.detected_concurrent_calls());
}

struct StubEmergencyCleanupRegistry : mir::EmergencyCleanupRegistry
{
    void add(mir::EmergencyCleanupHandler const& handler) override
    {
        this->handler = handler;
    }

    mir::EmergencyCleanupHandler handler;
};

TEST_F(MesaGraphicsPlatform, restores_vt_on_emergency_cleanup)
{
    using namespace testing;

    auto const mock_vt = std::make_shared<mtd::MockVirtualTerminal>();
    StubEmergencyCleanupRegistry emergency_cleanup_registry;
    mgm::Platform platform{
        mir::report::null_display_report(),
        mock_vt,
        emergency_cleanup_registry,
        mgm::BypassOption::allowed};

    EXPECT_CALL(*mock_vt, restore());

    emergency_cleanup_registry.handler();

    Mock::VerifyAndClearExpectations(mock_vt.get());
}

TEST_F(MesaGraphicsPlatform, releases_drm_on_emergency_cleanup)
{
    using namespace testing;

    auto const null_vt = std::make_shared<mtd::NullVirtualTerminal>();
    StubEmergencyCleanupRegistry emergency_cleanup_registry;
    mgm::Platform platform{
        mir::report::null_display_report(),
        null_vt,
        emergency_cleanup_registry,
        mgm::BypassOption::allowed};

    int const success_code = 0;
    EXPECT_CALL(mock_drm, drmDropMaster(mock_drm.fake_drm.fd()))
        .WillOnce(Return(success_code));

    emergency_cleanup_registry.handler();

    Mock::VerifyAndClearExpectations(&mock_drm);
}

TEST_F(MesaGraphicsPlatform, does_not_propagate_emergency_cleanup_exceptions)
{
    using namespace testing;

    auto const mock_vt = std::make_shared<mtd::MockVirtualTerminal>();
    StubEmergencyCleanupRegistry emergency_cleanup_registry;
    mgm::Platform platform{
        mir::report::null_display_report(),
        mock_vt,
        emergency_cleanup_registry,
        mgm::BypassOption::allowed};

    EXPECT_CALL(*mock_vt, restore())
        .WillOnce(Throw(std::runtime_error("vt restore exception")));
    EXPECT_CALL(mock_drm, drmDropMaster(mock_drm.fake_drm.fd()))
        .WillOnce(Throw(std::runtime_error("drm drop master exception")));

    emergency_cleanup_registry.handler();

    Mock::VerifyAndClearExpectations(&mock_drm);
}
