/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Nick Dedekind <nick.dedekind <nick.dedekind@canonical.com>
 */

#include "src/client/mir_trust_session.h"
#include "src/client/mir_event_distributor.h"

#include "mir_test/fake_shared.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>

namespace mcl = mir::client;
namespace mt = mir::test;

namespace google
{
namespace protobuf
{
class RpcController;
}
}

namespace
{

struct MockProtobufServer : mir::protobuf::DisplayServer
{

    MOCK_METHOD4(start_trust_session,
                 void(::google::protobuf::RpcController* /*controller*/,
                      ::mir::protobuf::TrustSessionParameters const* /*request*/,
                      ::mir::protobuf::TrustSession* /*response*/,
                      ::google::protobuf::Closure* /*done*/));

    MOCK_METHOD4(add_trusted_session,
                 void(::google::protobuf::RpcController* /*controller*/,
                      const ::mir::protobuf::TrustedSession* /*request*/,
                      ::mir::protobuf::TrustSessionAddResult* /*response*/,
                      ::google::protobuf::Closure* /*done*/));

    MOCK_METHOD4(stop_trust_session,
                 void(::google::protobuf::RpcController* /*controller*/,
                      ::mir::protobuf::Void const* /*request*/,
                      ::mir::protobuf::Void* /*response*/,
                      ::google::protobuf::Closure* /*done*/));
};

class StubProtobufServer : public mir::protobuf::DisplayServer
{
public:
    void start_trust_session(::google::protobuf::RpcController* /*controller*/,
                             ::mir::protobuf::TrustSessionParameters const* /*request*/,
                             ::mir::protobuf::TrustSession* response,
                             ::google::protobuf::Closure* done) override
    {
        if (server_thread.joinable())
            server_thread.join();
        server_thread = std::thread{
            [response, done, this]
            {
                response->clear_error();
                done->Run();
            }};
    }

    void add_trusted_session(::google::protobuf::RpcController* /*controller*/,
                             const ::mir::protobuf::TrustedSession* /*request*/,
                             ::mir::protobuf::TrustSessionAddResult* /*response*/,
                             ::google::protobuf::Closure* done)
    {
        if (server_thread.joinable())
            server_thread.join();
        server_thread = std::thread{[done, this] { done->Run(); }};
    }

    void stop_trust_session(::google::protobuf::RpcController* /*controller*/,
                            mir::protobuf::Void const* /*request*/,
                            ::mir::protobuf::Void* /*response*/,
                            ::google::protobuf::Closure* done) override
    {
        if (server_thread.joinable())
            server_thread.join();
        server_thread = std::thread{[done, this] { done->Run(); }};
    }

    StubProtobufServer()
    {
    }

    ~StubProtobufServer()
    {
        if (server_thread.joinable())
            server_thread.join();
    }

private:
    std::thread server_thread;
};

class MirTrustSessionTest : public testing::Test
{
public:
    MirTrustSessionTest()
    {
    }

    static void trust_session_event(MirTrustSession*, MirTrustSessionState new_state, void* context)
    {
        MirTrustSessionTest* test = static_cast<MirTrustSessionTest*>(context);
        test->state_updated(new_state);
    }

    MOCK_METHOD1(state_updated, void(MirTrustSessionState));

    testing::NiceMock<MockProtobufServer> mock_server;
    StubProtobufServer stub_server;
    MirEventDistributor event_distributor;
};

struct MockCallback
{
    MOCK_METHOD2(call, void(void*, void*));
};

void mock_callback_func(MirTrustSession* trust_session, void* context)
{
    auto mock_cb = static_cast<MockCallback*>(context);
    mock_cb->call(trust_session, context);
}

void null_callback_func(MirTrustSession*, void*)
{
}

ACTION(RunClosure)
{
    arg3->Run();
}

}

TEST_F(MirTrustSessionTest, start_trust_session)
{
    using namespace testing;

    EXPECT_CALL(mock_server,
                start_trust_session(_,_,_,_))
        .WillOnce(RunClosure());

    MirTrustSession trust_session{
        mock_server,
        mt::fake_shared(event_distributor)};
    trust_session.start(__LINE__, null_callback_func, nullptr);
}

TEST_F(MirTrustSessionTest, stop_trust_session)
{
    using namespace testing;

    EXPECT_CALL(mock_server,
                stop_trust_session(_,_,_,_))
        .WillOnce(RunClosure());

    MirTrustSession trust_session{
        mock_server,
        mt::fake_shared(event_distributor)};
    trust_session.stop(null_callback_func, nullptr);
}

TEST_F(MirTrustSessionTest, executes_callback_on_start)
{
    using namespace testing;

    MockCallback mock_cb;
    EXPECT_CALL(mock_cb, call(_, &mock_cb));

    MirTrustSession trust_session{
        stub_server,
        mt::fake_shared(event_distributor)};
    trust_session.start(__LINE__, mock_callback_func, &mock_cb)->wait_for_all();
}

TEST_F(MirTrustSessionTest, executes_callback_on_stop)
{
    using namespace testing;

    MockCallback mock_cb;
    EXPECT_CALL(mock_cb, call(_, &mock_cb));

    MirTrustSession trust_session{
        stub_server,
        mt::fake_shared(event_distributor)};
    trust_session.stop(mock_callback_func, &mock_cb)->wait_for_all();
}

TEST_F(MirTrustSessionTest, state_change_event_handler)
{
    using namespace testing;

    MirTrustSession trust_session{
        mock_server,
        mt::fake_shared(event_distributor)};
    trust_session.register_trust_session_event_callback(&MirTrustSessionTest::trust_session_event, this);

    EXPECT_CALL(*this, state_updated(mir_trust_session_state_started)).Times(1);

    MirEvent e;
    e.type = mir_event_type_trust_session_state_change;
    e.trust_session.new_state = mir_trust_session_state_started;
    event_distributor.handle_event(e);
}

