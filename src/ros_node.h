/// \file ros_node.h
/// \brief Defines the ros_node class.
#ifndef ROS_NODE_H
#define ROS_NODE_H

#include "driver.h"

#include <ros/ros.h>

#include <driver_modem_msgs/data_packet.h>
#include <driver_modem_msgs/set_remote_host.h>
#include <driver_modem_msgs/get_remote_host.h>
#include <driver_modem_msgs/add_tcp_connection.h>
#include <driver_modem_msgs/add_udp_connection.h>
#include <driver_modem_msgs/remove_connection.h>
#include <driver_modem_msgs/remove_all_connections.h>
#include <driver_modem_msgs/send_tcp.h>

/// \brief Implements the driver's ROS node functionality.
class ros_node
{
public:
    // CONSTRUCTORS
    /// \brief Initializes the ROS node.
    /// \param argc Number of main() args.
    /// \param argv The main() args.
    ros_node(int argc, char **argv);
    ~ros_node();

    // METHODS
    /// \brief Runs the node.
    void spin();

private:
    // VARIABLES
    /// \brief The driver instance.
    driver* m_driver;
    /// \brief The node's handle.
    ros::NodeHandle* m_node;

    // VARIABLES: PUBLISHERS
    /// \brief The publisher for ActiveConnection messages.
    ros::Publisher m_publisher_active_connections;
    /// \brief The map of TCP RX publishers.
    std::map<uint16_t, ros::Publisher> m_tcp_rx;
    /// \brief The map of UDP RX publishers.
    std::map<uint16_t, ros::Publisher> m_udp_rx;

    // VARIABLES: SUBSCRIBERS
    /// \brief The map of UDP TX subscribers.
    std::map<uint16_t, ros::Subscriber> m_udp_tx;
    /// \brief The map of TCP TX service servers.
    std::map<uint16_t, ros::ServiceServer> m_tcp_tx;

    // VARIABLES: SERVICES
    /// \brief Service for setting the driver's remote host.
    ros::ServiceServer m_service_set_remote_host;
    /// \brief Service for geting the driver's remote host.
    ros::ServiceServer m_service_get_remote_host;
    /// \brief Service for adding TCP connections.
    ros::ServiceServer m_service_add_tcp_connection;
    /// \brief Service for adding UDP connections.
    ros::ServiceServer m_service_add_udp_connection;
    /// \brief Service for removing TCP/UDP connections.
    ros::ServiceServer m_service_remove_connection;
    /// \brief Service for removing all connections.
    ros::ServiceServer m_service_remove_all_connections;

    // METHODS: CONNECTION MANAGEMENT
    /// \brief Sets the remote host of the modem and clears all current connections.
    /// \param remote_host The new remote host.
    /// \return TRUE if successful, otherwise FALSE.
    bool set_remote_host(std::string remote_host);
    /// \brief Instructs the driver to add a new TCP connection.
    /// \param role The role of the new TCP connection.
    /// \param port The port of the new TCP connection.
    /// \param publish_connections Indicates if the method should publish the active_connections method.
    /// \return TRUE if the new connection was added, otherwise FALSE.
    bool add_tcp_connection(tcp_role role, uint16_t port, bool publish_connections = true);
    /// \brief Instructs the driver to add a new UDP connection.
    /// \param port The port of the new UDP connection.
    /// \param publish_connections Indicates if the method should publish the active_connections method.
    /// \return TRUE if the new connection was added, otherwise FALSE.
    bool add_udp_connection(uint16_t port, bool publish_connections = true);
    /// \brief Instructs the driver to remove a TCP or UDP connection
    /// \param type The protocol type of the connection to remove.
    /// \param port The port of the connection to remove.
    /// \param publish_connections Indicates if the method should publish the active_connections method.
    /// \return TRUE if the connection was removed, otherwise FALSE.
    bool remove_connection(protocol type, uint16_t port, bool publish_connections = true);
    /// \brief Instructs the driver to remove all connections.
    void remove_all_connections(bool publish_connections = true);

    // METHODS: TOPIC MANAGEMENT
    /// \brief Sets up publishers, subscribers, and services for new connections.
    /// \param type The protocol type of connection added.
    /// \param port The port of the connection added.
    void add_connection_topics(protocol type, uint16_t port);
    /// \brief Removes publishers, subscribers, and services for closed connections.
    /// \param type The protocol type of connected removed.
    /// \param port The port of the connection removed.
    void remove_connection_topics(protocol type, uint16_t port);
    /// \brief Removes all publishers, subscribers, and services.
    void remove_connection_topics();

    // METHODS: MISC
    /// \brief Publishes active connections.
    void publish_active_connections();

    // CALLBACKS: DRIVER
    /// \brief The callback for handling driver TCP connection events.
    /// \param port The port of the new TCP connection.
    void callback_tcp_connected(uint16_t port);
    /// \brief The callback for handling driver TCP disconnection events.
    /// \param port The port of the closed TCP connection.
    void callback_tcp_disconnected(uint16_t port);
    /// \brief Handles the RX of data for all connections.
    /// \param type The connection type that received the data.
    /// \param port The local port that received the data.
    /// \param data The data that was received.
    /// \param length The length of the data that was received.
    /// \param source The IP address of the data source.
    void callback_rx(protocol type, uint16_t port, uint8_t* data, uint32_t length, address source);

    // CALLBACKS: SUBSCRIBERS
    /// \brief Forwards received data_packet messages from udp tx topics.
    /// \param message The message to forward.
    /// \param port The local port to forward the message to.
    void callback_udp_tx(const driver_modem_msgs::data_packetConstPtr& message, uint16_t port);

    // CALLBACKS: SERVICES
    /// \brief Service callback for setting the driver's remote host.
    /// \param request The service request.
    /// \param response The service response.
    /// \return TRUE if the service succeeded, otherwise FALSE.
    bool service_set_remote_host(driver_modem_msgs::set_remote_hostRequest& request, driver_modem_msgs::set_remote_hostResponse& response);
    /// \brief Service callback for getting the driver's remote host.
    /// \param request The service request.
    /// \param response The service response.
    /// \return TRUE if the service succeeded, otherwise FALSE.
    bool service_get_remote_host(driver_modem_msgs::get_remote_hostRequest& request, driver_modem_msgs::get_remote_hostResponse& response);
    /// \brief Service callback for adding TCP connectins.
    /// \param request The service request.
    /// \param response The service response.
    /// \return TRUE if the service succeeded, otherwise FALSE.
    bool service_add_tcp_connection(driver_modem_msgs::add_tcp_connectionRequest& request, driver_modem_msgs::add_tcp_connectionResponse& response);
    /// \brief Service callback for adding UDP connections.
    /// \param request The service request.
    /// \param response The service response.
    /// \return TRUE if the service succeeded, otherwise FALSE.
    bool service_add_udp_connection(driver_modem_msgs::add_udp_connectionRequest& request, driver_modem_msgs::add_udp_connectionResponse& response);
    /// \brief Service callback for removing TCP/UDP connections.
    /// \param request The service request.
    /// \param response The service response.
    /// \return TRUE if the service succeeded, otherwise FALSE.
    bool service_remove_connection(driver_modem_msgs::remove_connectionRequest& request, driver_modem_msgs::remove_connectionResponse& response);
    /// \brief Service callback for removing all active and pending connections.
    /// \param request The service request.
    /// \param response The service response.
    /// \return TRUE if the service succeeded, otherwise FALSE.
    bool service_remove_all_connections(driver_modem_msgs::remove_all_connectionsRequest& request, driver_modem_msgs::remove_all_connectionsResponse& response);
    /// \brief Service for transmitting data over a TCP connection.
    /// \param request The service request.
    /// \param response The service response.
    /// \param port The TCP port to communicate the data over.
    /// \return TRUE if the service succeeded, otherwise FALSE.
    bool service_tcp_tx(driver_modem_msgs::send_tcpRequest& request, driver_modem_msgs::send_tcpResponse& response, uint16_t port);
};

#endif // ROS_NODE_H

