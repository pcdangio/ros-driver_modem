/// \file driver.h
/// \brief Defines the driver class.
#ifndef DRIVER_H
#define DRIVER_H

#include "tcp_connection.h"
#include "udp_connection.h"

///
/// \brief A driver for TCP/UDP communications over a network interface.
///
class driver
{
public:
    ///
    /// \brief driver Instantiates a new driver instance.
    /// \param local_ip The local IP address to bind to.
    /// \param remote_ip The remote IP address to communicate with.
    /// \param rx_callback A callback function for handling received messages.
    /// \param disconnect_callback A callback function for handling disconnect events.
    ///
    driver(std::string local_ip, std::string remote_ip, std::function<void(connection_type, uint16_t, uint8_t*, uint32_t)> rx_callback, std::function<void(connection_type, uint16_t)> disconnect_callback);
    ~driver();

    ///
    /// \brief add_connection Adds a new TCP/UDP connection.
    /// \param type The type of connection to add (TCP or UDP).
    /// \param local_port The local port to listen on.
    /// \param remote_port The remote port to communicate with.
    /// \return TRUE if the connection was added, otherwise FALSE.
    ///
    bool add_connection(connection_type type, uint16_t local_port, uint16_t remote_port);
    ///
    /// \brief remove_connection Removes an existing TCP/UDP connection.
    /// \param type The type of connection to remove (TCP or UDP).
    /// \param local_port The local port of the connection.
    /// \return TRUE if the connection was removed, otherwise FALSE.
    ///
    bool remove_connection(connection_type type, uint16_t local_port);
    ///
    /// \brief spin_once Spins the driver for a single iteration, raising callbacks.
    ///
    void spin_once();

    ///
    /// \brief p_connections Gets a list of active connections.
    /// \return A std::vector of active connections.
    ///
    std::vector<std::pair<connection_type, uint16_t>> p_connections();

private:
    ///
    /// \brief m_service Stores the driver's io_service instance.
    ///
    boost::asio::io_service m_service;
    ///
    /// \brief m_tcp_connections Stores the map of current TCP connections.
    ///
    std::map<uint16_t, tcp_connection*> m_tcp_connections;
    ///
    /// \brief m_udp_connections Stores the map of current UDP connections.
    ///
    std::map<uint16_t, udp_connection*> m_udp_connections;
    ///
    /// \brief m_local_ip Stores the local IP address for all connections.
    ///
    boost::asio::ip::address m_local_ip;
    ///
    /// \brief m_remote_ip Stores the remote IP address for all connections.
    ///
    boost::asio::ip::address m_remote_ip;

    ///
    /// \brief m_rx_callback The callback for handling received messages.
    ///
    std::function<void(connection_type, uint16_t, uint8_t*, uint32_t)> m_rx_callback;
    ///
    /// \brief m_disconnect_callback The callback for handling disconnect events.
    ///
    std::function<void(connection_type, uint16_t)> m_disconnect_callback;

    ///
    /// \brief disconnect_callback The internal callback for handling TCP disconnect events.
    /// \param local_port The port of the TCP connection that has been disconnected.
    ///
    void disconnect_callback(uint16_t local_port);
};

#endif // DRIVER_H
