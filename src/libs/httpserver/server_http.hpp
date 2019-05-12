#ifndef SERVER_HTTP_HPP
#define SERVER_HTTP_HPP

#include "utility.hpp"
#include <functional>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_set>

// [CLIPPER]
#include <boost/thread.hpp>

#ifdef USE_STANDALONE_ASIO
#include <asio.hpp>
#include <asio/steady_timer.hpp>
namespace SimpleWeb {
  using error_code = std::error_code;
  using errc = std::errc;
  namespace make_error_code = std;
} // namespace SimpleWeb
#else
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
namespace SimpleWeb {
  namespace asio = boost::asio;
  using error_code = boost::system::error_code;
  namespace errc = boost::system::errc;
  namespace make_error_code = boost::system::errc;
} // namespace SimpleWeb
#endif

// Late 2017 TODO: remove the following checks and always use std::regex
#ifdef USE_BOOST_REGEX
#include <boost/regex.hpp>
namespace SimpleWeb {
  namespace regex = boost;
}
#else
#include <regex>
namespace SimpleWeb {
  namespace regex = std;
}
#endif

namespace SimpleWeb {
  template <class socket_type>
  class Server;

  template <class socket_type>
  class ServerBase {
  protected:
    class Session;

  public:
    class Response : public std::enable_shared_from_this<Response>, public std::ostream {
      friend class ServerBase<socket_type>;
      friend class Server<socket_type>;

      std::unique_ptr<asio::streambuf> streambuf = std::unique_ptr<asio::streambuf>(new asio::streambuf());

      std::shared_ptr<Session> session;
      long timeout_content;

      asio::io_service::strand strand;
      std::list<std::pair<std::shared_ptr<asio::streambuf>, std::function<void(const error_code &)>>> send_queue;

      Response(std::shared_ptr<Session> session_, long timeout_content) noexcept : std::ostream(nullptr), session(std::move(session_)), timeout_content(timeout_content), strand(session->connection->socket->get_io_service()) {
        rdbuf(streambuf.get());
      }

      template <typename size_type>
      void write_header(const CaseInsensitiveMultimap &header, size_type size) {
        bool content_length_written = false;
        bool chunked_transfer_encoding = false;
        for(auto &field : header) {
          if(!content_length_written && case_insensitive_equal(field.first, "content-length"))
            content_length_written = true;
          else if(!chunked_transfer_encoding && case_insensitive_equal(field.first, "transfer-encoding") && case_insensitive_equal(field.second, "chunked"))
            chunked_transfer_encoding = true;

          *this << field.first << ": " << field.second << "\r\n";
        }
        if(!content_length_written && !chunked_transfer_encoding && !close_connection_after_response)
          *this << "Content-Length: " << size << "\r\n\r\n";
        else
          *this << "\r\n";
      }

      void send_from_queue() {
        auto self = this->shared_from_this();
        strand.post([self]() {
          asio::async_write(*self->session->connection->socket, *self->send_queue.begin()->first, self->strand.wrap([self](const error_code &ec, std::size_t /*bytes_transferred*/) {
            auto lock = self->session->connection->handler_runner->continue_lock();
            if(!lock)
              return;
            if(!ec) {
              auto it = self->send_queue.begin();
              if(it->second)
                it->second(ec);
              self->send_queue.erase(it);
              if(self->send_queue.size() > 0)
                self->send_from_queue();
            }
            else {
              // All handlers in the queue is called with ec:
              for(auto &pair : self->send_queue) {
                if(pair.second)
                  pair.second(ec);
              }
              self->send_queue.clear();
            }
          }));
        });
      }

      void send_on_delete(const std::function<void(const error_code &)> &callback = nullptr) noexcept {
        session->connection->set_timeout(timeout_content);
        auto self = this->shared_from_this(); // Keep Response instance alive through the following async_write
        asio::async_write(*session->connection->socket, *streambuf, [self, callback](const error_code &ec, std::size_t /*bytes_transferred*/) {
          self->session->connection->cancel_timeout();
          auto lock = self->session->connection->handler_runner->continue_lock();
          if(!lock)
            return;
          if(callback)
            callback(ec);
        });
      }

    public:
      std::size_t size() noexcept {
        return streambuf->size();
      }

      /// Use this function if you need to recursively send parts of a longer message, or when using server-sent events (SSE).
      void send(const std::function<void(const error_code &)> &callback = nullptr) noexcept {
        session->connection->set_timeout(timeout_content);

        std::shared_ptr<asio::streambuf> streambuf = std::move(this->streambuf);
        this->streambuf = std::unique_ptr<asio::streambuf>(new asio::streambuf());
        rdbuf(this->streambuf.get());

        auto self = this->shared_from_this();
        strand.post([self, streambuf, callback]() {
          self->send_queue.emplace_back(streambuf, callback);
          if(self->send_queue.size() == 1)
            self->send_from_queue();
        });
      }

      /// Write directly to stream buffer using std::ostream::write
      void write(const char_type *ptr, std::streamsize n) {
        std::ostream::write(ptr, n);
      }

      /// Convenience function for writing status line, potential header fields, and empty content
      void write(StatusCode status_code = StatusCode::success_ok, const CaseInsensitiveMultimap &header = CaseInsensitiveMultimap()) {
        *this << "HTTP/1.1 " << SimpleWeb::status_code(status_code) << "\r\n";
        write_header(header, 0);
      }

      /// Convenience function for writing status line, header fields, and content
      void write(StatusCode status_code, string_view content, const CaseInsensitiveMultimap &header = CaseInsensitiveMultimap()) {
        *this << "HTTP/1.1 " << SimpleWeb::status_code(status_code) << "\r\n";
        write_header(header, content.size());
        if(!content.empty())
          *this << content;
      }

      /// Convenience function for writing status line, header fields, and content
      void write(StatusCode status_code, std::istream &content, const CaseInsensitiveMultimap &header = CaseInsensitiveMultimap()) {
        *this << "HTTP/1.1 " << SimpleWeb::status_code(status_code) << "\r\n";
        content.seekg(0, std::ios::end);
        auto size = content.tellg();
        content.seekg(0, std::ios::beg);
        write_header(header, size);
        if(size)
          *this << content.rdbuf();
      }

      /// Convenience function for writing success status line, header fields, and content
      void write(string_view content, const CaseInsensitiveMultimap &header = CaseInsensitiveMultimap()) {
        write(StatusCode::success_ok, content, header);
      }

      /// Convenience function for writing success status line, header fields, and content
      void write(std::istream &content, const CaseInsensitiveMultimap &header = CaseInsensitiveMultimap()) {
        write(StatusCode::success_ok, content, header);
      }

      /// Convenience function for writing success status line, and header fields
      void write(const CaseInsensitiveMultimap &header) {
        write(StatusCode::success_ok, std::string(), header);
      }

      /// If true, force server to close the connection after the response have been sent.
      ///
      /// This is useful when implementing a HTTP/1.0-server sending content
      /// without specifying the content length.
      bool close_connection_after_response = false;
    };

    class Content : public std::istream {
      friend class ServerBase<socket_type>;

    public:
      std::size_t size() noexcept {
        return streambuf.size();
      }
      /// Convenience function to return std::string. The stream buffer is consumed.
      std::string string() noexcept {
        try {
          std::string str;
          auto size = streambuf.size();
          str.resize(size);
          read(&str[0], static_cast<std::streamsize>(size));
          return str;
        }
        catch(...) {
          return std::string();
        }
      }

    private:
      asio::streambuf &streambuf;
      Content(asio::streambuf &streambuf) noexcept : std::istream(&streambuf), streambuf(streambuf) {}
    };

    class Request {
      friend class ServerBase<socket_type>;
      friend class Server<socket_type>;
      friend class Session;

      asio::streambuf streambuf;

      Request(std::size_t max_request_streambuf_size, std::shared_ptr<asio::ip::tcp::endpoint> remote_endpoint_) noexcept
          : streambuf(max_request_streambuf_size), content(streambuf), remote_endpoint(std::move(remote_endpoint_)) {}

    public:
      std::string method, path, query_string, http_version;

      Content content;

      CaseInsensitiveMultimap header;

      regex::smatch path_match;

      std::shared_ptr<asio::ip::tcp::endpoint> remote_endpoint;

      /// The time point when the request header was fully read.
      std::chrono::system_clock::time_point header_read_time;

      std::string remote_endpoint_address() const noexcept {
        try {
          return remote_endpoint->address().to_string();
        }
        catch(...) {
          return std::string();
        }
      }

      unsigned short remote_endpoint_port() const noexcept {
        return remote_endpoint->port();
      }

      /// Returns query keys with percent-decoded values.
      CaseInsensitiveMultimap parse_query_string() const noexcept {
        return SimpleWeb::QueryString::parse(query_string);
      }
    };

  protected:
    class Connection : public std::enable_shared_from_this<Connection> {
    public:
      template <typename... Args>
      Connection(std::shared_ptr<ScopeRunner> handler_runner_, Args &&... args) noexcept : handler_runner(std::move(handler_runner_)), socket(new socket_type(std::forward<Args>(args)...)) {}

      std::shared_ptr<ScopeRunner> handler_runner;

      std::unique_ptr<socket_type> socket; // Socket must be unique_ptr since asio::ssl::stream<asio::ip::tcp::socket> is not movable

      std::unique_ptr<asio::steady_timer> timer;

      std::shared_ptr<asio::ip::tcp::endpoint> remote_endpoint;

      void close() noexcept {
        error_code ec;
        socket->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        socket->lowest_layer().cancel(ec);
      }

      void set_timeout(long seconds) noexcept {
        if(seconds == 0) {
          timer = nullptr;
          return;
        }

        timer = std::unique_ptr<asio::steady_timer>(new asio::steady_timer(socket->get_io_service()));
        timer->expires_from_now(std::chrono::seconds(seconds));
        auto self = this->shared_from_this();
        timer->async_wait([self](const error_code &ec) {
          if(!ec)
            self->close();
        });
      }

      void cancel_timeout() noexcept {
        if(timer) {
          error_code ec;
          timer->cancel(ec);
        }
      }
    };

    class Session {
    public:
      Session(std::size_t max_request_streambuf_size, std::shared_ptr<Connection> connection_) noexcept : connection(std::move(connection_)) {
        if(!this->connection->remote_endpoint) {
          error_code ec;
          this->connection->remote_endpoint = std::make_shared<asio::ip::tcp::endpoint>(this->connection->socket->lowest_layer().remote_endpoint(ec));
        }
        request = std::shared_ptr<Request>(new Request(max_request_streambuf_size, this->connection->remote_endpoint));
      }

      std::shared_ptr<Connection> connection;
      std::shared_ptr<Request> request;
    };

  public:
    class Config {
      friend class ServerBase<socket_type>;

      Config(unsigned short port) noexcept : port(port) {}

      // [CLIPPER]
      Config(std::string address, unsigned short port) noexcept : port(port), address(address) {}
      Config(std::string address, unsigned short port, size_t thread_pool_size, long timeout_request, long timeout_content) noexcept :
        port(port), thread_pool_size(thread_pool_size), timeout_request(timeout_request), timeout_content(timeout_content), address(address) {}

    public:
      /// Port number to use. Defaults to 80 for HTTP and 443 for HTTPS. Set to 0 get an assigned port.
      unsigned short port;
      /// If io_service is not set, number of threads that the server will use when start() is called.
      /// Defaults to 1 thread.
      std::size_t thread_pool_size = 1;
      /// Timeout on request handling. Defaults to 5 seconds.
      long timeout_request = 5;
      /// Timeout on content handling. Defaults to 300 seconds.
      long timeout_content = 300;
      /// Maximum size of request stream buffer. Defaults to architecture maximum.
      /// Reaching this limit will result in a message_size error code.
      std::size_t max_request_streambuf_size = std::numeric_limits<std::size_t>::max();
      /// IPv4 address in dotted decimal form or IPv6 address in hexadecimal notation.
      /// If empty, the address will be any address.
      std::string address;
      /// Set to false to avoid binding the socket to an address that is already in use. Defaults to true.
      bool reuse_address = true;
      /// Make use of RFC 7413 or TCP Fast Open (TFO)
      bool fast_open = false;
    };
    /// Set before calling start().
    Config config;

  private:
    class regex_orderable : public regex::regex {
    public:
      std::string str;

      regex_orderable(const char *regex_cstr) : regex::regex(regex_cstr), str(regex_cstr) {}
      regex_orderable(std::string regex_str_) : regex::regex(regex_str_), str(std::move(regex_str_)) {}
      bool operator<(const regex_orderable &rhs) const noexcept {
        return str < rhs.str;
      }
    };

  public:
    // [CLIPPER]
    std::unordered_map<std::string, std::unordered_map<std::string, std::function<void(std::shared_ptr<typename ServerBase<socket_type>::Response>, std::shared_ptr<typename ServerBase<socket_type>::Request>)>>> resource;
    // std::map<regex_orderable, std::map<std::string, std::function<void(std::shared_ptr<typename ServerBase<socket_type>::Response>, std::shared_ptr<typename ServerBase<socket_type>::Request>)>>> resource;

    // [CLIPPER]
    std::unordered_map<std::string, std::function<void(std::shared_ptr<typename ServerBase<socket_type>::Response>, std::shared_ptr<typename ServerBase<socket_type>::Request>)>> default_resource;
    // std::map<std::string, std::function<void(std::shared_ptr<typename ServerBase<socket_type>::Response>, std::shared_ptr<typename ServerBase<socket_type>::Request>)>> default_resource;

    // [CLIPPER]
    std::vector<std::pair<std::string, std::vector<std::pair<regex::regex, std::function<void(std::shared_ptr<typename ServerBase<socket_type>::Response>, std::shared_ptr<typename ServerBase<socket_type>::Request>)>>>>> opt_resource;

    std::function<void(std::shared_ptr<typename ServerBase<socket_type>::Request>, const error_code &)> on_error;

    std::function<void(std::unique_ptr<socket_type> &, std::shared_ptr<typename ServerBase<socket_type>::Request>)> on_upgrade;

    /// If you have your own asio::io_service, store its pointer here before running start().
    std::shared_ptr<asio::io_service> io_service;

    /// If you know the server port in advance, use start() instead.
    /// Returns assigned port. If io_service is not set, an internal io_service is created instead.
    /// Call before accept_and_run().
    unsigned short bind() {
      asio::ip::tcp::endpoint endpoint;
      if(config.address.size() > 0)
        endpoint = asio::ip::tcp::endpoint(asio::ip::address::from_string(config.address), config.port);
      else
        endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v6(), config.port);

      if(!io_service) {
        io_service = std::make_shared<asio::io_service>();
        internal_io_service = true;
      }

      if(!acceptor)
        acceptor = std::unique_ptr<asio::ip::tcp::acceptor>(new asio::ip::tcp::acceptor(*io_service));
      acceptor->open(endpoint.protocol());
      acceptor->set_option(asio::socket_base::reuse_address(config.reuse_address));
      if(config.fast_open) {
#if defined(__linux__) && defined(TCP_FASTOPEN)
        const int qlen = 5; // This seems to be the value that is used in other examples.
        error_code ec;
        acceptor->set_option(asio::detail::socket_option::integer<IPPROTO_TCP, TCP_FASTOPEN>(qlen), ec);
#endif // End Linux
      }
      acceptor->bind(endpoint);

      after_bind();

      return acceptor->local_endpoint().port();
    }

    /// If you know the server port in advance, use start() instead.
    /// Accept requests, and if io_service was not set before calling bind(), run the internal io_service instead.
    /// Call after bind().
    void accept_and_run() {
      acceptor->listen();
      accept();

      if(internal_io_service) {
        if(io_service->stopped())
          io_service->reset();

        // If thread_pool_size>1, start m_io_service.run() in (thread_pool_size-1) threads for thread-pooling
        threads.clear();
        for(std::size_t c = 1; c < config.thread_pool_size; c++) {
          threads.emplace_back([this]() {
            this->io_service->run();
          });
        }

        // Main thread
        if(config.thread_pool_size > 0)
          io_service->run();

        // Wait for the rest of the threads, if any, to finish as well
        for(auto &t : threads)
          t.join();
      }
    }

    /// Start the server by calling bind() and accept_and_run()
    void start() {
      // [CLIPPER] Start
      // Copy the resources to opt_resource for more efficient request processing
      {
        boost::unique_lock<boost::shared_mutex> l(opt_resource_mutex);
        opt_resource.clear();
        for(auto& res: resource) {
          for(auto& res_method: res.second) {
            auto it = opt_resource.end();
            for(auto opt_it = opt_resource.begin(); opt_it != opt_resource.end(); opt_it++) {
              if(res_method.first == opt_it->first) {
                it = opt_it;
                break;
              }
            }
            if(it == opt_resource.end()) {
              opt_resource.emplace_back();
              it = opt_resource.begin() + int(opt_resource.size() - 1);
              it->first = res_method.first;
            }
            it->second.emplace_back(regex::regex(res.first), res_method.second);
          }
        }
      }
      // [CLIPPER] End

      bind();
      accept_and_run();
    }

    /// Stop accepting new requests, and close current connections.
    void stop() noexcept {
      if(acceptor) {
        error_code ec;
        acceptor->close(ec);

        {
          std::lock_guard<std::mutex> lock(*connections_mutex);
          for(auto &connection : *connections)
            connection->close();
          connections->clear();
        }

        if(internal_io_service)
          io_service->stop();
      }
    }

    virtual ~ServerBase() noexcept {
      handler_runner->stop();
      stop();
    }

  // [CLIPPER] Start
  /// Use this function to add new endpoints while the service is running
  void add_endpoint(std::string res_name, std::string res_method,
    std::function<void(std::shared_ptr<typename ServerBase<socket_type>::Response>,
                       std::shared_ptr<typename ServerBase<socket_type>::Request>)> res_fn) {
    boost::unique_lock<boost::shared_mutex> l(opt_resource_mutex);
    resource[res_name][res_method] = res_fn;
    auto it = opt_resource.end();
    for (auto opt_it = opt_resource.begin(); opt_it != opt_resource.end(); opt_it++) {
      if (res_method == opt_it->first) {
        it = opt_it;
        break;
      }
    }
    if (it == opt_resource.end()) {
      opt_resource.emplace_back();
      it = opt_resource.begin() + (opt_resource.size() - 1);
      it->first = res_method;
    }
    it->second.emplace_back(regex::regex(res_name), res_fn);
  }

  // Use this function to delete endpoints while the service is running
  void delete_endpoint(std::string res_name, std::string res_method) {
    boost::unique_lock<boost::shared_mutex> l(opt_resource_mutex);

    // Delete application name from resource map, if it exists
    if (resource.find(res_name) != resource.end()) {
      auto method_map = &resource[res_name];
      if (method_map->find(res_method) != method_map->end()) {
        auto to_delete = method_map->find(res_method);
        method_map->erase(to_delete);
      }
    }

    // Delete application name from opt_resource
    auto it = opt_resource.end();
    for (auto opt_it = opt_resource.begin(); opt_it != opt_resource.end();
         opt_it++) {
      if (res_method == opt_it->first) {
        it = opt_it;
        break;
      }
    }

    if (it != opt_resource.end()) {
      auto name_vec = &it->second;
      auto to_erase = name_vec->end();

      // Strip regex symbols (first and last characters) from resource name
      res_name = res_name.substr(1, res_name.size() - 2);

      for (auto name_it = name_vec->begin(); name_it != name_vec->end();
           name_it++) {
        regex::smatch sm_res;

        if(regex::regex_match(res_name, sm_res, name_it->first)) {
          to_erase = name_it;
          break;
        }
      }
      name_vec->erase(to_erase);
    }
  }

  size_t num_endpoints() {
    boost::shared_lock<boost::shared_mutex> l(opt_resource_mutex);
    size_t count = 0;
    for (auto e : opt_resource) {
      count += e.second.size();
    }
    return count;
  }
  // [CLIPPER] End

  protected:
    bool internal_io_service = false;

    std::unique_ptr<asio::ip::tcp::acceptor> acceptor;
    std::vector<std::thread> threads;

    // [CLIPPER]
    boost::shared_mutex opt_resource_mutex;

    std::shared_ptr<std::unordered_set<Connection *>> connections;
    std::shared_ptr<std::mutex> connections_mutex;

    std::shared_ptr<ScopeRunner> handler_runner;

    ServerBase(unsigned short port) noexcept : config(port), connections(new std::unordered_set<Connection *>()), connections_mutex(new std::mutex()), handler_runner(new ScopeRunner()) {}

    // [CLIPPER]
    ServerBase(std::string address, unsigned short port) noexcept :
      config(address, port),
      connections(new std::unordered_set<Connection *>()), connections_mutex(new std::mutex()), handler_runner(new ScopeRunner()) {}
    ServerBase(std::string address, unsigned short port, size_t thread_pool_size, long timeout_request, long timeout_content) noexcept :
      config(address, port, thread_pool_size, timeout_request, timeout_content),
      connections(new std::unordered_set<Connection *>()), connections_mutex(new std::mutex()), handler_runner(new ScopeRunner()) {}

    virtual void after_bind() {}
    virtual void accept() = 0;

    template <typename... Args>
    std::shared_ptr<Connection> create_connection(Args &&... args) noexcept {
      auto connections = this->connections;
      auto connections_mutex = this->connections_mutex;
      auto connection = std::shared_ptr<Connection>(new Connection(handler_runner, std::forward<Args>(args)...), [connections, connections_mutex](Connection *connection) {
        {
          std::lock_guard<std::mutex> lock(*connections_mutex);
          auto it = connections->find(connection);
          if(it != connections->end())
            connections->erase(it);
        }
        delete connection;
      });
      {
        std::lock_guard<std::mutex> lock(*connections_mutex);
        connections->emplace(connection.get());
      }
      return connection;
    }

    void read(const std::shared_ptr<Session> &session) {
      session->connection->set_timeout(config.timeout_request);
      asio::async_read_until(*session->connection->socket, session->request->streambuf, "\r\n\r\n", [this, session](const error_code &ec, std::size_t bytes_transferred) {
        session->connection->cancel_timeout();
        auto lock = session->connection->handler_runner->continue_lock();
        if(!lock)
          return;
        session->request->header_read_time = std::chrono::system_clock::now();
        if((!ec || ec == asio::error::not_found) && session->request->streambuf.size() == session->request->streambuf.max_size()) {
          auto response = std::shared_ptr<Response>(new Response(session, this->config.timeout_content));
          response->write(StatusCode::client_error_payload_too_large);
          if(this->on_error)
            this->on_error(session->request, make_error_code::make_error_code(errc::message_size));
          return;
        }
        if(!ec) {
          // request->streambuf.size() is not necessarily the same as bytes_transferred, from Boost-docs:
          // "After a successful async_read_until operation, the streambuf may contain additional data beyond the delimiter"
          // The chosen solution is to extract lines from the stream directly when parsing the header. What is left of the
          // streambuf (maybe some bytes of the content) is appended to in the async_read-function below (for retrieving content).
          std::size_t num_additional_bytes = session->request->streambuf.size() - bytes_transferred;

          if(!RequestMessage::parse(session->request->content, session->request->method, session->request->path,
                                    session->request->query_string, session->request->http_version, session->request->header)) {
            if(this->on_error)
              this->on_error(session->request, make_error_code::make_error_code(errc::protocol_error));
            return;
          }

          // If content, read that as well
          auto header_it = session->request->header.find("Content-Length");
          if(header_it != session->request->header.end()) {
            unsigned long long content_length = 0;
            try {
              content_length = stoull(header_it->second);
            }
            catch(const std::exception &) {
              if(this->on_error)
                this->on_error(session->request, make_error_code::make_error_code(errc::protocol_error));
              return;
            }
            if(content_length > num_additional_bytes) {
              session->connection->set_timeout(config.timeout_content);
              asio::async_read(*session->connection->socket, session->request->streambuf, asio::transfer_exactly(content_length - num_additional_bytes), [this, session](const error_code &ec, std::size_t /*bytes_transferred*/) {
                session->connection->cancel_timeout();
                auto lock = session->connection->handler_runner->continue_lock();
                if(!lock)
                  return;
                if(!ec) {
                  if(session->request->streambuf.size() == session->request->streambuf.max_size()) {
                    auto response = std::shared_ptr<Response>(new Response(session, this->config.timeout_content));
                    response->write(StatusCode::client_error_payload_too_large);
                    if(this->on_error)
                      this->on_error(session->request, make_error_code::make_error_code(errc::message_size));
                    return;
                  }
                  this->find_resource(session);
                }
                else if(this->on_error)
                  this->on_error(session->request, ec);
              });
            }
            else
              this->find_resource(session);
          }
          else if((header_it = session->request->header.find("Transfer-Encoding")) != session->request->header.end() && header_it->second == "chunked") {
            auto chunks_streambuf = std::make_shared<asio::streambuf>(this->config.max_request_streambuf_size);
            this->read_chunked_transfer_encoded(session, chunks_streambuf);
          }
          else
            this->find_resource(session);
        }
        else if(this->on_error)
          this->on_error(session->request, ec);
      });
    }

    void read_chunked_transfer_encoded(const std::shared_ptr<Session> &session, const std::shared_ptr<asio::streambuf> &chunks_streambuf) {
      session->connection->set_timeout(config.timeout_content);
      asio::async_read_until(*session->connection->socket, session->request->streambuf, "\r\n", [this, session, chunks_streambuf](const error_code &ec, size_t bytes_transferred) {
        session->connection->cancel_timeout();
        auto lock = session->connection->handler_runner->continue_lock();
        if(!lock)
          return;
        if((!ec || ec == asio::error::not_found) && session->request->streambuf.size() == session->request->streambuf.max_size()) {
          auto response = std::shared_ptr<Response>(new Response(session, this->config.timeout_content));
          response->write(StatusCode::client_error_payload_too_large);
          if(this->on_error)
            this->on_error(session->request, make_error_code::make_error_code(errc::message_size));
          return;
        }
        if(!ec) {
          std::string line;
          getline(session->request->content, line);
          bytes_transferred -= line.size() + 1;
          line.pop_back();
          unsigned long length = 0;
          try {
            length = stoul(line, 0, 16);
          }
          catch(...) {
            if(this->on_error)
              this->on_error(session->request, make_error_code::make_error_code(errc::protocol_error));
            return;
          }

          auto num_additional_bytes = session->request->streambuf.size() - bytes_transferred;

          if((2 + length) > num_additional_bytes) {
            session->connection->set_timeout(config.timeout_content);
            asio::async_read(*session->connection->socket, session->request->streambuf, asio::transfer_exactly(2 + length - num_additional_bytes), [this, session, chunks_streambuf, length](const error_code &ec, size_t /*bytes_transferred*/) {
              session->connection->cancel_timeout();
              auto lock = session->connection->handler_runner->continue_lock();
              if(!lock)
                return;
              if(!ec) {
                if(session->request->streambuf.size() == session->request->streambuf.max_size()) {
                  auto response = std::shared_ptr<Response>(new Response(session, this->config.timeout_content));
                  response->write(StatusCode::client_error_payload_too_large);
                  if(this->on_error)
                    this->on_error(session->request, make_error_code::make_error_code(errc::message_size));
                  return;
                }
                this->read_chunked_transfer_encoded_chunk(session, chunks_streambuf, length);
              }
              else if(this->on_error)
                this->on_error(session->request, ec);
            });
          }
          else
            this->read_chunked_transfer_encoded_chunk(session, chunks_streambuf, length);
        }
        else if(this->on_error)
          this->on_error(session->request, ec);
      });
    }

    void read_chunked_transfer_encoded_chunk(const std::shared_ptr<Session> &session, const std::shared_ptr<asio::streambuf> &chunks_streambuf, unsigned long length) {
      std::ostream tmp_stream(chunks_streambuf.get());
      if(length > 0) {
        std::unique_ptr<char[]> buffer(new char[length]);
        session->request->content.read(buffer.get(), static_cast<std::streamsize>(length));
        tmp_stream.write(buffer.get(), static_cast<std::streamsize>(length));
        if(chunks_streambuf->size() == chunks_streambuf->max_size()) {
          auto response = std::shared_ptr<Response>(new Response(session, this->config.timeout_content));
          response->write(StatusCode::client_error_payload_too_large);
          if(this->on_error)
            this->on_error(session->request, make_error_code::make_error_code(errc::message_size));
          return;
        }
      }

      // Remove "\r\n"
      session->request->content.get();
      session->request->content.get();

      if(length > 0)
        read_chunked_transfer_encoded(session, chunks_streambuf);
      else {
        if(chunks_streambuf->size() > 0) {
          std::ostream ostream(&session->request->streambuf);
          ostream << chunks_streambuf.get();
        }
        this->find_resource(session);
      }
    }

    void find_resource(const std::shared_ptr<Session> &session) {
      // Upgrade connection
      if(on_upgrade) {
        auto it = session->request->header.find("Upgrade");
        if(it != session->request->header.end()) {
          // remove connection from connections
          {
            std::lock_guard<std::mutex> lock(*connections_mutex);
            auto it = connections->find(session->connection.get());
            if(it != connections->end())
              connections->erase(it);
          }

          on_upgrade(session->connection->socket, session->request);
          return;
        }
      }
      // Find path- and method-match, and call write
      // [CLIPPER] Start
      {
        boost::unique_lock<boost::shared_mutex> l(opt_resource_mutex);
        for (auto& res: opt_resource) {
          if(session->request->method == res.first) {
            for(auto& res_path: res.second) {
              regex::smatch sm_res;
              if(regex::regex_match(session->request->path, sm_res, res_path.first)) {
                session->request->path_match = std::move(sm_res);
                write(session, res_path.second);
                return;
              }
            }
          }
        }
      }
      // for(auto &regex_method : resource) {
      //   auto it = regex_method.second.find(session->request->method);
      //   if(it != regex_method.second.end()) {
      //     regex::smatch sm_res;
      //     if(regex::regex_match(session->request->path, sm_res, regex_method.first)) {
      //       session->request->path_match = std::move(sm_res);
      //       write(session, it->second);
      //       return;
      //     }
      //   }
      // }
      // [CLIPPER] END
      auto it = default_resource.find(session->request->method);
      if(it != default_resource.end())
        write(session, it->second);
    }

    void write(const std::shared_ptr<Session> &session,
               std::function<void(std::shared_ptr<typename ServerBase<socket_type>::Response>, std::shared_ptr<typename ServerBase<socket_type>::Request>)> &resource_function) {
      session->connection->set_timeout(config.timeout_content);
      auto response = std::shared_ptr<Response>(new Response(session, config.timeout_content), [this](Response *response_ptr) {
        auto response = std::shared_ptr<Response>(response_ptr);
        response->send_on_delete([this, response](const error_code &ec) {
          if(!ec) {
            if(response->close_connection_after_response)
              return;

            auto range = response->session->request->header.equal_range("Connection");
            for(auto it = range.first; it != range.second; it++) {
              if(case_insensitive_equal(it->second, "close"))
                return;
              else if(case_insensitive_equal(it->second, "keep-alive")) {
                auto new_session = std::make_shared<Session>(this->config.max_request_streambuf_size, response->session->connection);
                this->read(new_session);
                return;
              }
            }
            if(response->session->request->http_version >= "1.1") {
              auto new_session = std::make_shared<Session>(this->config.max_request_streambuf_size, response->session->connection);
              this->read(new_session);
              return;
            }
          }
          else if(this->on_error)
            this->on_error(response->session->request, ec);
        });
      });

      try {
        resource_function(response, session->request);
      }
      catch(const std::exception &) {
        if(on_error)
          on_error(session->request, make_error_code::make_error_code(errc::operation_canceled));
        return;
      }
    }
  };

  template <class socket_type>
  class Server : public ServerBase<socket_type> {};

  using HTTP = asio::ip::tcp::socket;

  template <>
  class Server<HTTP> : public ServerBase<HTTP> {
  public:
    Server() noexcept : ServerBase<HTTP>::ServerBase(80) {}

    // [CLIPPER]
    Server(std::string address, unsigned short port) noexcept :
      ServerBase<HTTP>::ServerBase(address, port) {}
    Server(std::string address, unsigned short port, size_t thread_pool_size, long timeout_request, long timeout_content) noexcept :
      ServerBase<HTTP>::ServerBase(address, port, thread_pool_size, timeout_request, timeout_content) {}

  protected:
    void accept() override {
      auto connection = create_connection(*io_service);

      acceptor->async_accept(*connection->socket, [this, connection](const error_code &ec) {
        auto lock = connection->handler_runner->continue_lock();
        if(!lock)
          return;

        // Immediately start accepting a new connection (unless io_service has been stopped)
        if(ec != asio::error::operation_aborted)
          this->accept();

        auto session = std::make_shared<Session>(config.max_request_streambuf_size, connection);

        if(!ec) {
          asio::ip::tcp::no_delay option(true);
          error_code ec;
          session->connection->socket->set_option(option, ec);

          this->read(session);
        }
        else if(this->on_error)
          this->on_error(session->request, ec);
      });
    }
  };
} // namespace SimpleWeb

#endif /* SERVER_HTTP_HPP */
