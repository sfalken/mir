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
 * Authored by: Thomas Guest <thomas.guest@canonical.com>
 */

#include "mir_client/mir_client_library.h"
#include "mir_client/mir_rpc_channel.h"
#include "mir_client/mir_buffer_package.h" 

#include "mir_protobuf.pb.h"

#include <set>
#include <unordered_set>
#include <cstddef>

namespace mcl = mir::client;
namespace mp = mir::protobuf;
namespace gp = google::protobuf;

#ifdef MIR_USING_BOOST_THREADS
    using ::mir::std::condition_variable;
    using ::boost::unique_lock;
    using ::boost::lock_guard;
    using ::boost::thread;
    using ::boost::mutex;
#else
    using namespace std;
#endif

class MirSurface
{
public:
    MirSurface(MirSurface const &) = delete;
    MirSurface& operator=(MirSurface const &) = delete;

    MirSurface(
        mp::DisplayServer::Stub & server,
        MirSurfaceParameters const & params,
        mir_surface_lifecycle_callback callback, void * context)
        : server(server)
    {
        mir::protobuf::SurfaceParameters message;
        message.set_surface_name(params.name ? params.name : std::string());
        message.set_width(params.width);
        message.set_height(params.height);
        message.set_pixel_format(params.pixel_format);

        server.create_surface(0, &message, &surface, gp::NewCallback(callback, this, context));
    }
    void release(mir_surface_lifecycle_callback callback, void * context)
    {
        mir::protobuf::SurfaceId message;
        message.set_value(surface.id().value());
        server.release_surface(0, &message, &void_response,
                               gp::NewCallback(this, &MirSurface::released, callback, context));
    }

    MirSurfaceParameters get_parameters() const
    {
        return MirSurfaceParameters {
            0,
            surface.width(),
            surface.height(),
            static_cast<MirPixelFormat>(surface.pixel_format())};
    }

    char const * get_error_message()
    {
        if (surface.has_error())
        {
            return surface.error().c_str();
        }
        return error_message.c_str();
    }

    int id() const
    {
        return surface.id().value();
    }

    bool is_valid() const
    {
        return !surface.has_error();
    }

    void populate(mcl::MirBufferPackage& buffer_package)
    {
        if (is_valid() && surface.has_buffer())
        {
            auto const& buffer = surface.buffer();

            buffer_package.data.resize(buffer.data_size());
            for (int i = 0; i != buffer.data_size(); ++i)
                buffer_package.data[i] = buffer.data(i);

            buffer_package.fd.resize(buffer.fd_size());
            for (int i = 0; i != buffer.fd_size(); ++i)
                buffer_package.fd[i] = buffer.fd(i);
        }
    }

    void next_buffer(mir_surface_lifecycle_callback callback, void * context)
    {
        server.next_buffer(
            0,
            &surface.id(),
            surface.mutable_buffer(),
            google::protobuf::NewCallback(callback, this, context));
    }


private:

    void released(mir_surface_lifecycle_callback callback, void * context)
    {
        callback(this, context);
        delete this;
    }

    mp::DisplayServer::Stub & server;
    mp::Void void_response;
    mp::Surface surface;
    std::string error_message;
};

// TODO the connection should track all associated surfaces, and release them on
// disconnection.
class MirConnection
{
public:
    MirConnection(const std::string& socket_file,
        std::shared_ptr<mcl::Logger> const & log)
        : created(true),
          channel(socket_file, log)
        , server(&channel)
        , log(log)
    {
        {
            lock_guard<mutex> lock(connection_guard);
            valid_connections.insert(this);
        }
        connect_result.set_error("connect not called");
    }

    ~MirConnection()
    {
        {
            lock_guard<mutex> lock(connection_guard);
            valid_connections.erase(this);
        }
    }

    MirConnection(MirConnection const &) = delete;
    MirConnection& operator=(MirConnection const &) = delete;

    MirSurface* create_surface(
        MirSurfaceParameters const & params,
        mir_surface_lifecycle_callback callback,
        void * context)
    {
        return new MirSurface(server, params, callback, context);
    }

    char const * get_error_message()
    {
        if (connect_result.has_error())
        {
            return connect_result.error().c_str();
        }
        else
        {
        return error_message.c_str();
        }
    }

    void connect(
        const char* app_name,
        mir_connected_callback callback,
        void * context)
    {
        connect_parameters.set_application_name(app_name);
        server.connect(
            0,
            &connect_parameters,
            &connect_result,
            google::protobuf::NewCallback(callback, this, context));
    }

    void disconnect()
    {
        server.disconnect(
            0,
            &ignored,
            &ignored,
            google::protobuf::NewCallback(this, &MirConnection::done_disconnect));

        unique_lock<mutex> lock(guard);
        while (created) cv.wait(lock);
    }

    static bool is_valid(MirConnection *connection)
    {
        {
            lock_guard<mutex> lock(connection_guard);
            if (valid_connections.count(connection) == 0)
               return false;
        }

        return !connection->connect_result.has_error();
    }
private:
    void done_disconnect()
    {
        unique_lock<mutex> lock(guard);
        created = false;
        cv.notify_one();
    }

    mutex guard;
    condition_variable cv;
    bool created;

    mcl::MirRpcChannel channel;
    mp::DisplayServer::Stub server;
    std::shared_ptr<mcl::Logger> log;
    mp::Void void_response;
    mir::protobuf::Void connect_result;
    mir::protobuf::Void ignored;
    mir::protobuf::ConnectParameters connect_parameters;

    std::string error_message;
    std::set<MirSurface *> surfaces;

    static mutex connection_guard;
    static std::unordered_set<MirConnection *> valid_connections;
};

mutex MirConnection::connection_guard;
std::unordered_set<MirConnection *> MirConnection::valid_connections;

void mir_connect(char const* socket_file, char const* name, mir_connected_callback callback, void * context)
{

    try
    {
        auto log = std::make_shared<mcl::ConsoleLogger>();
        MirConnection * connection = new MirConnection(socket_file, log);
        connection->connect(name, callback, context);
    }
    catch (std::exception const& /*x*/)
    {
        // TODO callback with an error connection
    }
}

int mir_connection_is_valid(MirConnection * connection)
{
    return MirConnection::is_valid(connection);
}

char const * mir_connection_get_error_message(MirConnection * connection)
{
    return connection->get_error_message();
}

void mir_connection_release(MirConnection * connection)
{
    connection->disconnect();
    delete connection;
}

void mir_surface_create(MirConnection * connection,
                        MirSurfaceParameters const * params,
                        mir_surface_lifecycle_callback callback,
                        void * context)
{
    try
    {
        connection->create_surface(*params, callback, context);
    }
    catch (std::exception const&)
    {
        // TODO callback with an error surface
    }
}

void mir_surface_release(MirSurface * surface,
                         mir_surface_lifecycle_callback callback, void * context)
{
    surface->release(callback, context);
}

int mir_debug_surface_id(MirSurface * surface)
{
    return surface->id();
}

int mir_surface_is_valid(MirSurface* surface)
{
    return surface->is_valid();
}

char const * mir_surface_get_error_message(MirSurface * surface)
{
    return surface->get_error_message();
}

void mir_surface_get_parameters(MirSurface * surface, MirSurfaceParameters *parameters)
{
    *parameters = surface->get_parameters();
}

#if 0
void mir_surface_get_current_buffer(MirSurface *surface, MirBufferPackage *buffer_package)
{
    surface->populate(*buffer_package);
}
#endif
void mir_surface_next_buffer(MirSurface *surface, mir_surface_lifecycle_callback callback, void * context)
{
    surface->next_buffer(callback, context);
}

