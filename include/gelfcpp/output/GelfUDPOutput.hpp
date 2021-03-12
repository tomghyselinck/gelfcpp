#pragma once

#include <gelfcpp/GelfMessage.hpp>
#include <gelfcpp/detail/GelfSerializer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#include <string>
#include <cstdint>
#include <thread>


namespace gelfcpp
{
namespace output
{
/**
 * \brief Writes GelfMessages to an UDP socket
 *
 * Serializes and sends messages to e.g. an Graylog Node, sending is non-blocking.
 *
 * Uses chunking for big messages due to size limitations of UDP.
 * Additionally uses compression if enabled (by setting \c GELFCPP_WITH_COMPRESSION in CMake).
 */
class GelfUDPOutput
{
public:
    /**
     * \brief Creates an new output socket to given host and port
     *
     * \param host remote hostname
     * \param port remote UDP port
     */
    GelfUDPOutput(const std::string& host, uint16_t port)
        : serializer_()
        , endpoint_()
        , socket_()
        , io_service_()
        , work_(io_service_)
        // Start running the io_service.  The work_ object will keep
        // io_service::run() from returning even if there is no real work
        // queued into the io_service.
        // All credits to https://stackoverflow.com/a/32273702
        , work_thread_([this](){this->io_service_.run();})
    {
        boost::asio::ip::udp::resolver resolver(io_service_);
        boost::asio::ip::udp::resolver::query query(host, std::to_string(port));
        endpoint_ = *resolver.resolve(query);
        socket_.reset(new boost::asio::ip::udp::socket(io_service_, endpoint_.protocol()));
    }

    ~GelfUDPOutput() noexcept
    {
        // Explicitly stop the io_service.  Queued handlers will not be ran.
        io_service_.stop();

        // Synchronize with the work thread.
        work_thread_.join();
    }

    /**
     * \brief Serializes a message to the UDP stream
     *
     * \param message the message
     */
    void Write(const GelfMessage& message)
    {
        for (const std::string& chunk : serializer_.Serialize(message))
        {
            socket_->async_send_to(boost::asio::buffer(chunk), endpoint_, [](const boost::system::error_code&, unsigned long int) {});
        }
    }

private:
    detail::GelfSerializer serializer_;
    boost::asio::ip::udp::endpoint endpoint_;
    std::unique_ptr<boost::asio::ip::udp::socket> socket_;

    boost::asio::io_service io_service_;
    boost::asio::io_service::work work_;
    std::thread work_thread_;
};

}
}
