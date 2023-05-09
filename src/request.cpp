
#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WIN32_WINDOWS 0x0A00 // Windows 10
#endif 
#include <fstream>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <spdlog/spdlog.h>
#include "request.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <optional>
#ifndef _WIN32
int memcpy_s(
    void* dst,
    size_t sizeInBytes,
    const void* src,
    size_t count
)
{
    if (count == 0)
    {
        return 0;
    }
    if (src == NULL || sizeInBytes < count)
    {
        memset(dst, 0, sizeInBytes);
        return EINVAL;
    }

    memcpy(dst, src, count);
    return 0;
}
#endif

using namespace boost::asio;
namespace http = boost::beast::http;
using namespace std::chrono;
using boost::asio::ip::tcp;

template<>
struct fmt::formatter<boost::string_view> : fmt::formatter<std::string>
{
    auto format(boost::string_view my, format_context& ctx) -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "{}", std::string_view(my.data(), my.size()));
    }
};

struct Connection
{
    std::unique_ptr<boost::asio::io_context> ioc;
    std::unique_ptr<ssl::context> ctx;
    std::unique_ptr<ssl::stream<tcp::socket>> stream;
    std::string serverName;

    Connection() {

    }

    template<typename T>
    ssl::stream<tcp::socket>& send_request(const std::string& serverName, T sender /*void (*)(ssl::stream<tcp::socket>&)*/) {
        if (serverName != this->serverName) {
            this->serverName = serverName;
            setup();
        }

        try {
     
            sender(*stream);
            return *stream;
        }
        catch (...) {

        }
        // write failed, reopen connection
        setup();

        sender(*stream);
        return *stream;
    } 

    void setup() {
        stream.reset();
        ctx.reset();
        ioc.reset();

        ioc = std::make_unique<boost::asio::io_context>();
        ctx = std::make_unique<ssl::context>(ssl::context::sslv23_client);
        stream = std::make_unique<ssl::stream<tcp::socket>>(*ioc, *ctx);

        if (!SSL_set_tlsext_host_name(stream->native_handle(), serverName.c_str()))
        {
            boost::system::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
            throw boost::system::system_error{ ec };
        }

        tcp::resolver resolver{ *ioc };
        // Look up the domain name
        auto const results = resolver.resolve(serverName.c_str(), "443");
        // Make the connection on the IP address we get from a lookup
        boost::asio::connect(stream->next_layer(), results.begin(), results.end());

        // Perform the SSL handshake
        stream->handshake(ssl::stream_base::client);
    }
};

static Connection current_connection;

std::vector<uint8_t> GetFile(const std::string& serverName,
    const std::string& getCommand)
{
    auto start = high_resolution_clock::now();
    std::vector<uint8_t>  buffer;
  //  try
    {
        auto sender = [&](ssl::stream<tcp::socket>& stream) {
            boost::asio::streambuf request;
            std::ostream request_stream(&request);

            request_stream << "GET " << getCommand << " HTTP/1.1\r\n";
            request_stream << "Host: " << serverName << "\r\n";
            request_stream << "Accept: */*\r\n";
            request_stream << "Connection: keep-alive\r\n\r\n";
            // Send the request.
            boost::asio::write(stream, request);
            spdlog::info("Write took {}ms", duration_cast<milliseconds>(high_resolution_clock::now() - start).count());

            // Declare a container to hold the response
            http::response<http::dynamic_body> res;
            boost::beast::flat_buffer res_buffer;
            // Receive the HTTP response
            http::read(stream, res_buffer, res);
            auto& body = res.body();

            buffer.resize(body.size());
            size_t off = 0;
            for (auto buff : body.data()) {

                memcpy_s(buffer.data() + off, buffer.size() - off, buff.data(), buff.size());
                off += buff.size();
            }
            if (body.size() != off) {
                spdlog::critical("GetFile sizes differ: {} {}", body.size(), off);
            }

            spdlog::info("Responce PL:{} KA:{} L:{} T:{}", res.payload_size().get_value_or(-1), res.keep_alive(), res.at("content-length"), res.at("content-type"));

        };

        current_connection.send_request(serverName, sender);


        // Send the request.
   
   
      //  stream.shutdown();

    }
  //  catch (const std::exception& e)
    {
   //     spdlog::critical("GetFile failed {}", e.what());
    }
    return buffer;
}

Pix* load_image_from_url(std::string_view url)
{
    auto start = high_resolution_clock::now();
    auto domain_begin = url.find("//");
    auto path_begin = url.find("/", domain_begin + 2);
    if (domain_begin == std::string::npos || path_begin == std::string::npos) return nullptr;
    std::string serverName = std::string(url.begin() + domain_begin + 2, url.begin() + path_begin);

    std::string getCommand = std::string(url.substr(path_begin));

    auto buffer = GetFile(serverName, getCommand);
    auto got_buffer = high_resolution_clock::now();
    std::cout << "[DBG] load_image_from_url finished GetFile in" << duration_cast<milliseconds>(got_buffer - start).count() << "ms\n";

    auto* ret = pixReadMemPng(buffer.data(), buffer.size());
    std::cout << "[DBG] load_image_from_url finished pixReadMemPng in" << duration_cast<milliseconds>(high_resolution_clock::now() - got_buffer).count() << "ms\n";
    return ret;
}
