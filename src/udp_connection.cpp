#include "udp_connection.h"

#include <boost/bind.hpp>

// CONSTRUCTORS
udp_connection::udp_connection(boost::asio::io_service& io_service, udp::endpoint local_endpoint, udp::endpoint remote_endpoint, uint32_t buffer_size)
    // Initialize socket.
    :m_socket(io_service, local_endpoint)
{
    // Dynamically allocate buffer.
    udp_connection::m_buffer_size = buffer_size;
    udp_connection::m_buffer = new uint8_t[buffer_size];

    // Store remote endpoint.
    udp_connection::m_remote_endpoint = remote_endpoint;
}
udp_connection::~udp_connection()
{
    delete [] udp_connection::m_buffer;
}

// METHODS
void udp_connection::connect()
{
    // Start asynchronous rx.
    udp_connection::async_rx();
}
void udp_connection::disconnect()
{
    // Close the socket to stop all async operations.
    udp_connection::m_socket.close();
}

void udp_connection::attach_rx_callback(std::function<void(protocol, uint16_t, uint8_t *, uint32_t, address)> callback)
{
    udp_connection::m_rx_callback = callback;
}
void udp_connection::tx(const uint8_t *data, uint32_t length)
{
    udp_connection::m_socket.send_to(boost::asio::buffer(data, length), udp_connection::m_remote_endpoint);
}
void udp_connection::async_rx()
{
    // Start asynchronous receive, and store the source endpoint in m_remote_endpoint.
    udp_connection::m_socket.async_receive_from(boost::asio::buffer(udp_connection::m_buffer, udp_connection::m_buffer_size),
                                                udp_connection::m_remote_endpoint,
                                                boost::bind(&udp_connection::rx_callback, udp_connection::shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

// CALLBACKS
void udp_connection::rx_callback(const boost::system::error_code &error, std::size_t bytes_read)
{
    // Make sure there are no errors, and that the rx callback is attached.
    if(!error && udp_connection::m_rx_callback)
    {
        // Deep copy the data into a new output array.
        uint8_t* output_array = new uint8_t[bytes_read];
        std::memcpy(output_array, udp_connection::m_buffer, bytes_read);

        // Raise the callback.
        // NOTE: async_recieve_from stores the remote_endpoint in m_remote_endpoint.
        udp_connection::m_rx_callback(protocol::UDP,
                                      udp_connection::m_socket.local_endpoint().port(),
                                      output_array, static_cast<uint32_t>(bytes_read),
                                      udp_connection::m_remote_endpoint.address());

        // Start a new asynchronous receive.
        udp_connection::async_rx();
    }
    else
    {
        if(error != boost::asio::error::operation_aborted)
        {
            throw std::runtime_error("udp_connection::rx_callback: " + error.message());
        }
    }
}
