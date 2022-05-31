#pragma once

#include "json.h"
#include "stl.h"
#include <memory>
#include <functional>

namespace rpc {

class Service {
  public:
    Service() = default;
    virtual ~Service() = default;

    typedef std::function<void(Json&, Json&)> Fun;

    virtual const char* name() const = 0;
    virtual const co::map<const char*, Fun>& methods() const = 0;
};

class __coapi Server {
  public:
    Server();
    ~Server();

    /**
     * add a service 
     *   - Multiple services can be added. 
     */
    void add_service(const std::shared_ptr<Service>& s);

    /**
     * add a service, @s MUST be created with operator new.
     */
    void add_service(Service* s) {
        this->add_service(std::shared_ptr<Service>(s));
    }

    /**
     * start the rpc server 
     *   - By default, key and ca are NULL, and ssl is disabled.
     * 
     * @param ip    server ip, either an ipv4 or ipv6 address.
     * @param port  server port
     * @param url   the url used to access the HTTP server, MUST begins with '/'
     * @param key   path of ssl private key file.
     * @param ca    path of ssl certificate file.
     */
    void start(const char* ip, int port, const char* url="/",
               const char* key=0, const char* ca=0);

    /**
     * exit the server gracefully
     *   - Once `exit()` was called, the listening socket will be closed, and new 
     *     connections will not be accepted.
     *   - NOTE: The server will not close previously established connections.
     */
    void exit();

  private:
    void* _p;

    DISALLOW_COPY_AND_ASSIGN(Server);
};

class __coapi Client {
  public:
    Client(const char* ip, int port, bool use_ssl=false);
    Client(const Client& c);
    ~Client();

    void operator=(const Client& c) = delete;

    // perform a rpc request
    void call(const Json& req, Json& res);

    // send a heartbeat
    void ping();

    // close the connection 
    void close();

  private:
    void* _p;
};

} // rpc
