/*
 * Copyright © 2016 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 or 3 as
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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIRAL_TEST_SERVER_H
#define MIRAL_TEST_SERVER_H

#include <miral/minimal_window_manager.h>
#include <miral/runner.h>
#include <miral/window_manager_tools.h>

#include <mir/test/auto_unblock_thread.h>
#include <mir_test_framework/temporary_environment_value.h>

#include <gtest/gtest.h>

#include <condition_variable>
#include <list>
#include <mutex>

namespace mir { namespace shell { class WindowManager; }}
namespace mir { namespace client { class Connection; }}
namespace miral
{
class WindowManagementPolicy;
class TestRuntimeEnvironment
{
public:
    void add_to_environment(char const* key, char const* value);

private:
    std::list<mir_test_framework::TemporaryEnvironmentValue> env;
};

struct TestDisplayServer : private TestRuntimeEnvironment
{
    TestDisplayServer();
    virtual ~TestDisplayServer();

    /// Add an environment variable for the duration of the test run
    using TestRuntimeEnvironment::add_to_environment;

    /// Add a callback to be invoked when the server has started,
    /// If multiple callbacks are added they will be invoked in the sequence added.
    /// \note call before start_server()
    void add_start_callback(std::function<void()> const& start_callback);

    /// Add a callback to be invoked when the server is about to stop,
    /// If multiple callbacks are added they will be invoked in the reverse sequence added.
    /// \note call before start_server()
    void add_stop_callback(std::function<void()> const& stop_callback);

    /// Set a handler for exceptions caught in run_with().
    /// The default action is to call mir::report_exception(std::cerr)
    /// \note call before start_server()
    void set_exception_handler(std::function<void()> const& handler);

    /// Add configuration code to be passed to runner.run_with() by start_server()
    /// \note call before start_server()
    void add_server_init(std::function<void(mir::Server&)>&& init);

    /// Start the server
    /// \note Typically called by TestServer::SetUp()
    void start_server();

    /// Get a connection for a mirclient
    /// \note call after start_server()
    auto connect_client(std::string name) -> mir::client::Connection;

    /// Wrapper to gain access to WindowManagerTools API (with correct locking in place)
    /// \note call after start_server()
    void invoke_tools(std::function<void(WindowManagerTools& tools)> const& f);

    /// Wrapper to gain access to WindowManager API (with correct locking in place)
    /// \note call after start_server()
    void invoke_window_manager(std::function<void(mir::shell::WindowManager& wm)> const& f);

    /// Stop the server
    /// \note Typically called by TestServer::TearDown()
    void stop_server();

    struct TestWindowManagerPolicy;
    virtual auto build_window_manager_policy(WindowManagerTools const& tools) -> std::unique_ptr<TestWindowManagerPolicy>;

private:
    MirRunner runner;

    WindowManagerTools tools{nullptr};
    std::weak_ptr<mir::shell::WindowManager> window_manager;
    mir::test::AutoJoinThread server_thread;
    std::mutex mutex;
    std::condition_variable started;
    mir::Server* server_running{nullptr};
    std::function<void(mir::Server&)> init_server = [](auto&){};
};

struct TestServer : TestDisplayServer, testing::Test
{
    bool start_server_in_setup = true;

    // Start the server (unless start_server_in_setup is false)
    void SetUp() override;

    // Stop the server
    void TearDown() override;
};

struct TestDisplayServer::TestWindowManagerPolicy : MinimalWindowManager
{
    TestWindowManagerPolicy(WindowManagerTools const& tools, TestDisplayServer& test_fixture);
};

}

#endif //MIRAL_TEST_SERVER_H
