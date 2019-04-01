/*
   The MIT License (MIT)

   Copyright (c) 2014-2016 Ole Christian Eidheim

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

#ifndef SERVER_HTTP_HPP
#define SERVER_HTTP_HPP

#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio.hpp>
#include <boost/functional/hash.hpp>

#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>

#ifndef CASE_INSENSITIVE_EQUALS_AND_HASH
#define CASE_INSENSITIVE_EQUALS_AND_HASH
//Based on http://www.boost.org/doc/libs/1_60_0/doc/html/unordered/hash_equality.html
class case_insensitive_equals {
public:
  bool operator()(const std::string &key1, const std::string &key2) const {
    return boost::algorithm::iequals(key1, key2);
  }
};
class case_insensitive_hash {
public:
  size_t operator()(const std::string &key) const {
    std::size_t seed=0;
    for(auto &c: key)
      boost::hash_combine(seed, std::tolower(c));
    return seed;
  }
};
#endif

// Late 2017 TODO: remove the following checks and always use std::regex
#ifdef USE_BOOST_REGEX
#include <boost/regex.hpp>
#define REGEX_NS boost
#else
#include <regex>
#define REGEX_NS std
#endif

// TODO when switching to c++14, use [[deprecated]] instead
#ifndef DEPRECATED
#ifdef __GNUC__
#define DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#define DEPRECATED __declspec(deprecated)
#else
#define DEPRECATED
#endif
#endif

namespace SimpleWeb {
template <class socket_type>
class Server;

template <class socket_type>
class ServerBase {
 public:
  virtual ~ServerBase() {}

  class Response : public std::ostream {
    friend class ServerBase<socket_type>;

    boost::asio::streambuf streambuf;

    std::shared_ptr<socket_type> socket;

    Response(const std::shared_ptr<socket_type> &socket)
        : std::ostream(&streambuf), socket(socket) {}

   public:
    size_t size() { return streambuf.size(); }

    /// If true, force server to close the connection after the response have been sent.
    ///
    /// This is useful when implementing a HTTP/1.0-server sending content
    /// without specifying the content length.
    bool close_connection_after_response = false;
  };

  class Content : public std::istream {
    friend class ServerBase<socket_type>;

   public:
    size_t size() { return streambuf.size(); }
    std::string string() {
      std::stringstream ss;
      ss << rdbuf();
      return ss.str();
    }

   private:
    boost::asio::streambuf &streambuf;
    Content(boost::asio::streambuf &streambuf)
        : std::istream(&streambuf), streambuf(streambuf) {}
  };

  class Request {
    friend class ServerBase<socket_type>;
    friend class Server<socket_type>;

   public:
    std::string method, path, http_version;

    Content content;

    std::unordered_multimap<std::string, std::string, case_insensitive_hash, case_insensitive_equals> header;

    REGEX_NS::smatch path_match;

    std::string remote_endpoint_address;
    unsigned short remote_endpoint_port;

    /// Returns query keys with percent-decoded values.
    std::unordered_multimap<std::string, std::string, case_insensitive_hash, case_insensitive_equals> parse_query_string() {
        std::unordered_multimap<std::string, std::string, case_insensitive_hash, case_insensitive_equals> result;
        auto qs_start_pos = path.find('?');
        if (qs_start_pos != std::string::npos && qs_start_pos + 1 < path.size()) {
            ++qs_start_pos;
            static REGEX_NS::regex pattern("([\\w+%]+)=?([^&]*)");
            int submatches[] = {1, 2};
            auto it_begin = REGEX_NS::sregex_token_iterator(path.begin() + qs_start_pos, path.end(), pattern, submatches);
            auto it_end = REGEX_NS::sregex_token_iterator();
            for (auto it = it_begin; it != it_end; ++it) {
                auto submatch1=it->str();
                auto submatch2=(++it)->str();
                auto query_it = result.emplace(submatch1, submatch2);
                auto &value = query_it->second;
                for (size_t c = 0; c < value.size(); ++c) {
                    if (value[c] == '+')
                        value[c] = ' ';
                    else if (value[c] == '%' && c + 2 < value.size()) {
                        auto hex = value.substr(c + 1, 2);
                        auto chr = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
                        value.replace(c, 3, &chr, 1);
                    }
                }
            }
        }
        return result;
    }

   private:
    Request(const socket_type &socket): content(streambuf) {
        try {
            remote_endpoint_address = socket.lowest_layer().remote_endpoint().address().to_string();
            remote_endpoint_port = socket.lowest_layer().remote_endpoint().port();
        }
        catch (...) {}
    }

    boost::asio::streambuf streambuf;
  };

  class Config {
    friend class ServerBase<socket_type>;

    Config(std::string address, unsigned short port): address(address), port(port) {}
    Config(std::string address, unsigned short port, size_t thread_pool_size, size_t timeout_request, size_t timeout_content):
        address(address), port(port), thread_pool_size(thread_pool_size), timeout_request(timeout_request), timeout_content(timeout_content) {}

   public:
    /// Port number to use. Defaults to 80 for HTTP and 443 for HTTPS.
    unsigned short port;
    /// Number of threads that the server will use when start() is called. Defaults to 1 thread.
    size_t thread_pool_size=1;
    /// Timeout on request handling. Defaults to 5 seconds.
    size_t timeout_request=5;
    /// Timeout on content handling. Defaults to 300 seconds.
    size_t timeout_content=300;
    /// IPv4 address in dotted decimal form or IPv6 address in hexadecimal
    /// notation. If empty, the address will be any address.
    std::string address;
    /// Set to false to avoid binding the socket to an address that is already in use. Defaults to true.
    bool reuse_address=true;
  };
  /// Set before calling start().
  Config config;

  public:
    std::unordered_map<
      std::string,
      std::unordered_map<
          std::string,
          std::function<void(
              std::shared_ptr<typename ServerBase<socket_type>::Response>,
              std::shared_ptr<typename ServerBase<socket_type>::Request>)> > >
      resource;

    std::unordered_map<
      std::string,
      std::function<void(
          std::shared_ptr<typename ServerBase<socket_type>::Response>,
          std::shared_ptr<typename ServerBase<socket_type>::Request>)> >
      default_resource;

    std::function<void(std::shared_ptr<typename ServerBase<socket_type>::Request>,
    const boost::system::error_code&)> on_error;

    std::function<void(std::shared_ptr<socket_type> socket,
    std::shared_ptr<typename ServerBase<socket_type>::Request>)> on_upgrade;

  private:
    std::vector<std::pair<
      std::string,
      std::vector<std::pair<
          REGEX_NS::regex,
          std::function<void(
              std::shared_ptr<typename ServerBase<socket_type>::Response>,
              std::shared_ptr<
                  typename ServerBase<socket_type>::Request>)> > > > >
      opt_resource;

 public:
  virtual void start() {
    //Copy the resources to opt_resource for more efficient request processing
    opt_resource.clear();
    for(auto& res: resource) {
        for(auto& res_method: res.second) {
            auto it=opt_resource.end();
            for(auto opt_it=opt_resource.begin();opt_it!=opt_resource.end();opt_it++) {
                if(res_method.first==opt_it->first) {
                    it=opt_it;
                    break;
                }
            }
            if(it==opt_resource.end()) {
                opt_resource.emplace_back();
                it=opt_resource.begin()+(opt_resource.size()-1);
                it->first=res_method.first;
            }
            it->second.emplace_back(REGEX_NS::regex(res.first), res_method.second);
        }
    }

    if (!io_service) io_service = std::make_shared<boost::asio::io_service>();

    if (io_service->stopped()) io_service->reset();

    boost::asio::ip::tcp::endpoint endpoint;
    if (config.address.size() > 0)
      endpoint = boost::asio::ip::tcp::endpoint(
          boost::asio::ip::address::from_string(config.address), config.port);
    else
      endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(),
                                                config.port);

    if (!acceptor)
      acceptor = std::unique_ptr<boost::asio::ip::tcp::acceptor>(
          new boost::asio::ip::tcp::acceptor(*io_service));
    acceptor->open(endpoint.protocol());
    acceptor->set_option(
        boost::asio::socket_base::reuse_address(config.reuse_address));
    acceptor->bind(endpoint);
    acceptor->listen();

    accept();

    //If thread_pool_size>1, start m_io_service.run() in (thread_pool_size-1) threads for thread-pooling
    threads.clear();
    for (size_t c = 1; c < config.thread_pool_size; c++) {
      threads.emplace_back([this]() { io_service->run(); });
    }

    // Main thread
    if (config.thread_pool_size > 0) io_service->run();

    // Wait for the rest of the threads, if any, to finish as well
    for (auto &t : threads) {
      t.join();
    }
  }

  void stop() {
    acceptor->close();
    if (config.thread_pool_size > 0) io_service->stop();
  }

  /// Use this function if you need to recursively send parts of a longer
  /// message
  void send(const std::shared_ptr<Response> &response,
            const std::function<void(const boost::system::error_code &)>
                &callback = nullptr) const {
    boost::asio::async_write(
        *response->socket, response->streambuf,
        [this, response, callback](const boost::system::error_code &ec,
                                   size_t /*bytes_transferred*/) {
          if (callback) callback(ec);
        });
  }

  /// If you have your own boost::asio::io_service, store its pointer here
  /// before running start(). You might also want to set config.thread_pool_size
  /// to 0.
  std::shared_ptr<boost::asio::io_service> io_service;

  /// Use this function to add new endpoints while the service is running
  void add_endpoint(
      std::string res_name, std::string res_method,
      std::function<
          void(std::shared_ptr<typename ServerBase<socket_type>::Response>,
               std::shared_ptr<typename ServerBase<socket_type>::Request>)>
          res_fn) {
    std::unique_lock<std::mutex> l(mu);
    resource[res_name][res_method] = res_fn;
    auto it = opt_resource.end();
    for (auto opt_it = opt_resource.begin(); opt_it != opt_resource.end();
         opt_it++) {
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
    it->second.emplace_back(REGEX_NS::regex(res_name), res_fn);
  }

  // Use this function to delete endpoints while the service is running
  void delete_endpoint(std::string res_name, std::string res_method) {
    std::unique_lock<std::mutex> l(mu);

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
        REGEX_NS::smatch sm_res;

        if(REGEX_NS::regex_match(res_name, sm_res, name_it->first)) {
          to_erase = name_it;
          break;
        }
      }
      name_vec->erase(to_erase);
    }
  }

  size_t num_endpoints() {
    size_t count = 0;
    std::unique_lock<std::mutex> l(mu);
    for (auto e : opt_resource) {
      count += e.second.size();
    }
    return count;
  }
  // } maybe added by clipper

 protected:
  std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor;
  std::vector<std::thread> threads;
  std::mutex mu;

  ServerBase(std::string address, unsigned short port) : config(address, port){}
  ServerBase(std::string address, unsigned short port, size_t thread_pool_size, size_t timeout_request, size_t timeout_content)
      : config(address, port, thread_pool_size, timeout_request, timeout_content) {}

  virtual void accept() = 0;

  std::shared_ptr<boost::asio::deadline_timer> get_timeout_timer(
      const std::shared_ptr<socket_type> &socket, long seconds) {
    if(seconds==0)
      return nullptr;

    auto timer=std::make_shared<boost::asio::deadline_timer>(*io_service);
    timer->expires_from_now(boost::posix_time::seconds(seconds));
    timer->async_wait([socket](const boost::system::error_code &ec) {
      if (!ec) {
        boost::system::error_code ec;
        socket->lowest_layer().shutdown(
            boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket->lowest_layer().close();
      }
    });
    return timer;
  }

  void read_request_and_content(const std::shared_ptr<socket_type> &socket) {
    // Create new streambuf (Request::streambuf) for async_read_until()
    // shared_ptr is used to pass temporary objects to the asynchronous
    // functions
    std::shared_ptr<Request> request(new Request(*socket));

    // Set timeout on the following boost::asio::async-read or write function
    auto timer=this->get_timeout_timer(socket, config.timeout_request);

    boost::asio::async_read_until(
        *socket, request->streambuf, "\r\n\r\n",
        [this, socket, request, timer](const boost::system::error_code &ec,
                                       size_t bytes_transferred) {
          if (timer) timer->cancel();
          if (!ec) {
            // request->streambuf.size() is not necessarily the same as
            // bytes_transferred, from Boost-docs: "After a successful
            //async_read_until operation, the streambuf may contain additional
            //data beyond the delimiter" The chosen solution is to extract lines
            // from the stream directly when parsing the header. What is left of
            // the streambuf (maybe some bytes of the content) is appended to in
            // the async_read-function below (for retrieving content).
            size_t num_additional_bytes =
                request->streambuf.size() - bytes_transferred;

            if (!this->parse_request(request)) return;

            // If content, read that as well
            auto it = request->header.find("Content-Length");
            if (it != request->header.end()) {
              unsigned long long content_length;
              try {
                content_length = stoull(it->second);
              } catch (const std::exception &e) {
                if (on_error)
                    on_error(request,
                        boost::system::error_code(
                            boost::system::errc::protocol_error,
                            boost::system::generic_category()));
                return;
              }
              if (content_length > num_additional_bytes) {
                //Set timeout on the following boost::asio::async-read or write function
                auto timer=this->get_timeout_timer(socket, config.timeout_content);
                boost::asio::async_read(
                    *socket, request->streambuf,
                    boost::asio::transfer_exactly(content_length -
                                                  num_additional_bytes),
                    [this, socket, request, timer](
                        const boost::system::error_code &ec,
                        size_t /*bytes_transferred*/) {
                      if (timer) timer->cancel();
                      if (!ec) this->find_resource(socket, request);
                      else if (on_error)
                        on_error(request, ec);
                    });
              } else
                this->find_resource(socket, request);
            } else
              this->find_resource(socket, request);
          }
          else if (on_error)
            on_error(request, ec);
        });
  }

  bool parse_request(const std::shared_ptr<Request> &request) const {
    std::string line;
    getline(request->content, line);
    size_t method_end;
    if ((method_end = line.find(' ')) != std::string::npos) {
      size_t path_end;
      if ((path_end = line.find(' ', method_end + 1)) != std::string::npos) {
        request->method = line.substr(0, method_end);
        request->path = line.substr(method_end + 1, path_end - method_end - 1);

        size_t protocol_end;
        if ((protocol_end = line.find('/', path_end + 1)) !=
            std::string::npos) {
          if(line.compare(path_end+1, protocol_end-path_end-1, "HTTP")!=0)
            return false;
          request->http_version =
              line.substr(protocol_end + 1, line.size() - protocol_end - 2);
        } else
          return false;

        getline(request->content, line);
        size_t param_end;
        while ((param_end = line.find(':')) != std::string::npos) {
          size_t value_start = param_end + 1;
          if ((value_start) < line.size()) {
            if (line[value_start] == ' ') value_start++;
            if (value_start < line.size())
              request->header.emplace(
                  line.substr(0, param_end),
                  line.substr(value_start, line.size() - value_start - 1));
          }

          getline(request->content, line);
        }
      } else
        return false;
    } else
      return false;
    return true;
  }

  void find_resource(const std::shared_ptr<socket_type> &socket,
                     const std::shared_ptr<Request> &request) {
    //Upgrade connection
    if(on_upgrade) {
        auto it=request->header.find("Upgrade");
        if(it!=request->header.end()) {
            on_upgrade(socket, request);
            return;
        }
    }
    // Find path- and method-match, and call write_response
    for (auto& res: opt_resource) {
      if(request->method==res.first) {
        for(auto& res_path: res.second) {
          REGEX_NS::smatch sm_res;
          if(REGEX_NS::regex_match(request->path, sm_res, res_path.first)) {
            request->path_match = std::move(sm_res);
            write_response(socket, request, res_path.second);
            return;
          }
        }
      }
    }
    auto it = default_resource.find(request->method);
    if (it != default_resource.end()) {
      write_response(socket, request, it->second);
    }
  }

  void write_response(
      const std::shared_ptr<socket_type> &socket,
      const std::shared_ptr<Request> &request,
      std::function<
          void(std::shared_ptr<typename ServerBase<socket_type>::Response>,
               std::shared_ptr<typename ServerBase<socket_type>::Request>)>
          &resource_function) {
    // Set timeout on the following boost::asio::async-read or write function
    auto timer=this->get_timeout_timer(socket, config.timeout_content);

    auto response = std::shared_ptr<Response>(
        new Response(socket), [this, request, timer](Response *response_ptr) {
          auto response = std::shared_ptr<Response>(response_ptr);
          this->send(response, [this, response, request,
                          timer](const boost::system::error_code &ec) {
            if (timer) timer->cancel();
            if (!ec) {
              if (response->close_connection_after_response)
                return;

              auto range = request->header.equal_range("Connection");
              for (auto it = range.first; it != range.second; it++) {
                if (boost::iequals(it->second, "close")) { return; }
                else if (boost::iequals(it->second, "keep-alive")) {
                  this->read_request_and_content(response->socket);
                  return;
                }
              }
              if (request->http_version >= "1.1")
                this->read_request_and_content(response->socket);
            }
            else if(on_error)
                on_error(request, ec);
          });
        });

    try {
      resource_function(response, request);
    } catch (const std::exception &e) {
      if (on_error)
          on_error(request,
              boost::system::error_code(
                  boost::system::errc::operation_canceled,
                  boost::system::generic_category()));
      return;
    }
  }
};

template <class socket_type>
class Server : public ServerBase<socket_type> {};

typedef boost::asio::ip::tcp::socket HTTP;

template <>
class Server<HTTP> : public ServerBase<HTTP> {
 public:
   Server(std::string address, unsigned short port) : ServerBase<HTTP>::ServerBase(address, port) {}
   Server(std::string address, unsigned short port, size_t thread_pool_size, size_t timeout_request, size_t timeout_content) :
       ServerBase<HTTP>::ServerBase(address, port, thread_pool_size, timeout_request, timeout_content) {}

 protected:
  void accept() {
    // Create new socket for this connection
    // Shared_ptr is used to pass temporary objects to the asynchronous
    // functions
    auto socket=std::make_shared<HTTP>(*io_service);

    acceptor->async_accept(*socket,
                           [this, socket](const boost::system::error_code &ec) {
                             //Immediately start accepting a new connection (if io_service hasn't been stopped)
                             if (ec != boost::asio::error::operation_aborted)
                               accept();

                             if (!ec) {
                               boost::asio::ip::tcp::no_delay option(true);
                               socket->set_option(option);

                               this->read_request_and_content(socket);
                             }
                             else if(on_error)
                                on_error(std::shared_ptr<Request>(new Request(*socket)), ec);
                           });
  }
};
}
#endif /* SERVER_HTTP_HPP */
