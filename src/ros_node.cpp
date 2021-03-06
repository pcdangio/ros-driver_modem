#include "ros_node.h"

#include <driver_modem_msgs/active_connections.h>

// CONSTRUCTORS
ros_node::ros_node(int argc, char **argv)
{
    // Initialize the ROS node.
    ros::init(argc, argv, "driver_modem");

    // Get the node's handle.
    ros_node::m_node = new ros::NodeHandle("~");

    // Read standard parameters.
    std::string param_local_ip;
    ros_node::m_node->param<std::string>("local_ip", param_local_ip, "192.168.1.2");
    std::string param_remote_host;
    ros_node::m_node->param<std::string>("remote_host", param_remote_host, "192.168.1.3");

    // Read connect port parameters.
    std::vector<int> param_tcp_server_ports;
    ros_node::m_node->getParam("tcp_server_ports", param_tcp_server_ports);
    std::vector<int> param_tcp_client_ports;
    ros_node::m_node->getParam("tcp_client_ports", param_tcp_client_ports);
    std::vector<int> param_udp_ports;
    ros_node::m_node->getParam("udp_ports", param_udp_ports);

    // Initialize driver.
    try
    {
        ros_node::m_driver = new driver(param_local_ip,
                                        param_remote_host,
                                        std::bind(&ros_node::callback_rx, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5),
                                        std::bind(&ros_node::callback_tcp_connected, this, std::placeholders::_1),
                                        std::bind(&ros_node::callback_tcp_disconnected, this, std::placeholders::_1));
    }
    catch (std::exception& e)
    {
        ROS_FATAL_STREAM(e.what());
        delete ros_node::m_node;
        exit(1);
    }


    // Set up active connections publisher.
    // This will publish each time the connections are modified.
    // Use latching so new nodes always have the latest information.
    ros_node::m_publisher_active_connections = ros_node::m_node->advertise<driver_modem_msgs::active_connections>("active_connections", 1, true);

    // Set up service for setting/getting remote host.
    ros_node::m_service_set_remote_host = ros_node::m_node->advertiseService("set_remote_host", &ros_node::service_set_remote_host, this);
    ros_node::m_service_get_remote_host = ros_node::m_node->advertiseService("get_remote_host", &ros_node::service_get_remote_host, this);

    // Set up services for adding/removing connections.
    ros_node::m_service_add_tcp_connection = ros_node::m_node->advertiseService("add_tcp_connection", &ros_node::service_add_tcp_connection, this);
    ros_node::m_service_add_udp_connection = ros_node::m_node->advertiseService("add_udp_connection", &ros_node::service_add_udp_connection, this);
    ros_node::m_service_remove_connection = ros_node::m_node->advertiseService("remove_connection", &ros_node::service_remove_connection, this);
    ros_node::m_service_remove_all_connections = ros_node::m_node->advertiseService("remove_all_connections", &ros_node::service_remove_all_connections, this);

    // Set up tx/rx publishers, subscribers, and services.

    // TCP Servers:
    for(uint32_t i = 0; i < param_tcp_server_ports.size(); i++)
    {
        // Get port from vector.
        uint16_t port = static_cast<uint16_t>(param_tcp_server_ports.at(i));

        // Add connection to node.
        ros_node::add_tcp_connection(tcp_role::SERVER, port, false);
    }

    // TCP Clients:
    for(uint32_t i = 0; i < param_tcp_client_ports.size(); i++)
    {
        // Get port from vector.
        uint16_t port = static_cast<uint16_t>(param_tcp_client_ports.at(i));

        // Add connection to node.
        ros_node::add_tcp_connection(tcp_role::CLIENT, port, false);
    }

    // UDP:
    for(uint32_t i = 0; i < param_udp_ports.size(); i++)
    {
        // Get local/remote ports from vectors.
        uint16_t port = static_cast<uint16_t>(param_udp_ports.at(i));

        // Add connection to node.
        ros_node::add_udp_connection(port, false);
    }

    // Manually publish connections after group add.
    ros_node::publish_active_connections();

    ROS_INFO_STREAM("Modem initialized." << std::endl << "Local IP:\t" << param_local_ip << std::endl << "Remote Host:\t" << param_remote_host);
}
ros_node::~ros_node()
{
    // Clean up resources.
    delete ros_node::m_node;
    delete ros_node::m_driver;
}

// PUBLIC METHODS
void ros_node::spin()
{
    // Start the driver thread.
    ros_node::m_driver->start();

    // Spin ROS node.
    ros::spin();

    // Stop the driver thread.
    ros_node::m_driver->stop();
}

// PRIVATE METHODS: CONNECTION MANAGEMENT
bool ros_node::set_remote_host(std::string remote_host)
{
    if(ros_node::m_driver->set_remote_host(remote_host))
    {
        // Remove all connection topics.
        ros_node::remove_connection_topics();

        // Publish active connections.
        ros_node::publish_active_connections();

        ROS_INFO_STREAM("Remote host set to " << remote_host << " and all connections closed");

        return true;
    }
    else
    {
        ROS_ERROR_STREAM("Could not set remote host to " << remote_host);

        return false;
    }
}
bool ros_node::add_tcp_connection(tcp_role role, uint16_t port, bool publish_connections)
{
    if(ros_node::m_driver->add_tcp_connection(role, port))
    {
        if(publish_connections)
        {
            // Publish active connections, since connection will initially be in PENDING status.
            ros_node::publish_active_connections();
        }

        // NOTE: TCP will update topics once becoming active through signal on connected tcp callback.

        ROS_INFO_STREAM("Connection added on TCP:" << port << " (" << driver::tcp_role_string(role) << ")");

        return true;
    }
    else
    {
        ROS_ERROR_STREAM("Could not add connection on TCP:" << port << " (" << driver::tcp_role_string(role) << ")");
        return false;
    }
}
bool ros_node::add_udp_connection(uint16_t port, bool publish_connections)
{
    if(ros_node::m_driver->add_udp_connection(port))
    {
        // Add UDP topic.
        ros_node::add_connection_topics(protocol::UDP, port);

        if(publish_connections)
        {
            // Publish active connections.
            ros_node::publish_active_connections();
        }

        ROS_INFO_STREAM("Connection added on UDP:" << port);

        return true;
    }
    else
    {
        ROS_ERROR_STREAM("Could not add connection on UDP:" << port);
        return false;
    }
}
bool ros_node::remove_connection(protocol type, uint16_t port, bool publish_connections)
{
    // Instruct driver to remove connection.
    if(ros_node::m_driver->remove_connection(type, port))
    {
        // Remove topics.
        // NOTE: For TCP, driver will not generate disconnected callbacks when driver::remove_connection() is called.
        ros_node::remove_connection_topics(type, port);

        if(publish_connections)
        {
            // Publish active connections.
            ros_node::publish_active_connections();
        }

        ROS_INFO_STREAM("Connection removed from " << driver::protocol_string(type) << ":" << port);

        return true;
    }
    else
    {
        ROS_ERROR_STREAM("Could not remove connection " << driver::protocol_string(type) << ":" << port);
        return false;
    }
}
void ros_node::remove_all_connections(bool publish_connections)
{
    // Remove all connection topics.
    ros_node::remove_connection_topics();

    // Remove connections from driver.
    ros_node::m_driver->remove_all_connections();

    // Signal if required.
    if(publish_connections)
    {
        // Publish active connections.
        ros_node::publish_active_connections();
    }

    ROS_INFO_STREAM("Removed all active and pending connections.");
}

// PRIVATE METHODS: TOPIC MANAGEMENT
void ros_node::add_connection_topics(protocol type, uint16_t port)
{
    switch(type)
    {
    case protocol::TCP:
    {
        // RX Publisher:
        // Generate topic name.
        std::stringstream rx_topic;
        rx_topic << "tcp/" << port << "/rx";
        // Add new rx publisher to the map.
        ros_node::m_tcp_rx.insert(std::make_pair(port, ros_node::m_node->advertise<driver_modem_msgs::data_packet>(rx_topic.str(), 1)));

        // TX Service:
        // Generate topic name.
        std::stringstream tx_topic;
        tx_topic << "tcp/" << port << "/tx";
        // Add new tx service server to the map.
        ros_node::m_tcp_tx.insert(std::make_pair(port, ros_node::m_node->advertiseService<driver_modem_msgs::send_tcpRequest, driver_modem_msgs::send_tcpResponse>(tx_topic.str(), std::bind(&ros_node::service_tcp_tx, this, std::placeholders::_1, std::placeholders::_2, port))));

        break;
    }
    case protocol::UDP:
    {
        // RX Publisher:
        // Generate topic name.
        std::stringstream rx_topic;
        rx_topic << "udp/" << port << "/rx";
        // Add new rx publisher to the map.
        ros_node::m_udp_rx.insert(std::make_pair(port, ros_node::m_node->advertise<driver_modem_msgs::data_packet>(rx_topic.str(), 1)));

        // TX Subscriber:
        // Generate topic name.
        std::stringstream tx_topic;
        tx_topic << "udp/" << port << "/tx";
        // Add new tx subscriber to the map.
        ros_node::m_udp_tx.insert(std::make_pair(port, ros_node::m_node->subscribe<driver_modem_msgs::data_packet>(tx_topic.str(), 1, std::bind(&ros_node::callback_udp_tx, this, std::placeholders::_1, port))));

        break;
    }
    }
}
void ros_node::remove_connection_topics(protocol type, uint16_t port)
{
    // Remove publishers/subscribers/callbacks of the connection.
    switch(type)
    {
    case protocol::TCP:
    {
        // Remove RX publisher
        if(ros_node::m_tcp_rx.count(port) > 0)
        {
            // Cancel topic.
            ros_node::m_tcp_rx.at(port).shutdown();
            // Remove from map.
            ros_node::m_tcp_rx.erase(port);
        }
        // Remove TX service
        if(ros_node::m_tcp_tx.count(port) > 0)
        {
            // Cancel service.
            ros_node::m_tcp_tx.at(port).shutdown();
            // Remove from map.
            ros_node::m_tcp_tx.erase(port);
        }
        break;
    }
    case protocol::UDP:
    {
        // Remove RX publisher
        if(ros_node::m_udp_rx.count(port) > 0)
        {
            // Cancel topic.
            ros_node::m_udp_rx.at(port).shutdown();
            // Remove from map.
            ros_node::m_udp_rx.erase(port);
        }
        // Remove TX subscriber
        if(ros_node::m_udp_tx.count(port) > 0)
        {
            // Cancel service.
            ros_node::m_udp_tx.at(port).shutdown();
            // Remove from map.
            ros_node::m_udp_tx.erase(port);
        }
        break;
    }
    }
}
void ros_node::remove_connection_topics()
{
    // Remove all TCP and UDP topics.
    for(auto it = ros_node::m_tcp_rx.begin(); it != ros_node::m_tcp_rx.end(); it++)
    {
        it->second.shutdown();
    }
    ros_node::m_tcp_rx.clear();
    for(auto it = ros_node::m_tcp_tx.begin(); it != ros_node::m_tcp_tx.end(); it++)
    {
        it->second.shutdown();
    }
    ros_node::m_tcp_tx.clear();
    for(auto it = ros_node::m_udp_rx.begin(); it != ros_node::m_udp_rx.end(); it++)
    {
        it->second.shutdown();
    }
    ros_node::m_udp_rx.clear();
    for(auto it = ros_node::m_udp_tx.begin(); it != ros_node::m_udp_tx.end(); it++)
    {
        it->second.shutdown();
    }
    ros_node::m_udp_tx.clear();
}

// PRIVATE METHODS: MISC
void ros_node::publish_active_connections()
{
    // Convert current connections into active_connections message.

    // Create output message.
    driver_modem_msgs::active_connections message;

    // Get the list of active connections from the driver.
    std::vector<uint16_t> pending_tcp = ros_node::m_driver->p_pending_tcp_connections();
    std::vector<uint16_t> active_tcp = ros_node::m_driver->p_active_tcp_connections();
    std::vector<uint16_t> active_udp = ros_node::m_driver->p_active_udp_connections();

    // Iterate over each connection type to populate message.
    for(uint32_t i = 0; i < pending_tcp.size(); i++)
    {
        message.tcp_pending.push_back(pending_tcp.at(i));
    }
    for(uint32_t i = 0; i < active_tcp.size(); i++)
    {
        message.tcp_active.push_back(active_tcp.at(i));
    }
    for(uint32_t i = 0; i < active_udp.size(); i++)
    {
        message.udp_active.push_back(active_udp.at(i));
    }

    // Publish the message.
    ros_node::m_publisher_active_connections.publish(message);
}

// CALLBACKS: DRIVER
void ros_node::callback_tcp_connected(uint16_t port)
{
    // TCP has transitioned from pending to active.

    // Add the associated topic/service.
    ros_node::add_connection_topics(protocol::TCP, port);

    // Publish updated connections.
    ros_node::publish_active_connections();

    ROS_INFO_STREAM("TCP:" << port << " connected.");
}
void ros_node::callback_tcp_disconnected(uint16_t port)
{
    // Remove the associated topic/service.  Driver has already internally removed connection.
    ros_node::remove_connection_topics(protocol::TCP, port);

    // Publish updated connections.
    ros_node::publish_active_connections();

    ROS_INFO_STREAM("TCP:" << port << " disconnected.");
}
void ros_node::callback_rx(protocol type, uint16_t port, uint8_t *data, uint32_t length, address source)
{
    // Deep copy data into new data_packet message.
    driver_modem_msgs::data_packet message;
    message.source_ip = source.to_string();
    for(uint32_t i = 0; i < length; i++)
    {
        message.data.push_back(data[i]);
    }
    // Clear raw data array.
    delete [] data;

    // Publish received message.
    switch(type)
    {
    case protocol::TCP:
    {
        if(ros_node::m_tcp_rx.count(port))
        {
            ros_node::m_tcp_rx.at(port).publish(message);
        }
        break;
    }
    case protocol::UDP:
    {
        if(ros_node::m_udp_rx.count(port))
        {
            ros_node::m_udp_rx.at(port).publish(message);
        }
        break;
    }
    }
}

// CALLBACKS: SUBSCRIBERS
void ros_node::callback_udp_tx(const driver_modem_msgs::data_packetConstPtr &message, uint16_t port)
{
    ros_node::m_driver->tx(protocol::UDP, port, message->data.data(), static_cast<uint32_t>(message->data.size()));
}

// CALLBACKS: SERVICES
bool ros_node::service_set_remote_host(driver_modem_msgs::set_remote_hostRequest &request, driver_modem_msgs::set_remote_hostResponse &response)
{
    response.success = ros_node::set_remote_host(request.remote_host);

    return true;
}
bool ros_node::service_get_remote_host(driver_modem_msgs::get_remote_hostRequest &request, driver_modem_msgs::get_remote_hostResponse &response)
{
    response.remote_host = ros_node::m_driver->p_remote_host();

    return true;
}
bool ros_node::service_add_tcp_connection(driver_modem_msgs::add_tcp_connectionRequest& request, driver_modem_msgs::add_tcp_connectionResponse& response)
{
    response.success = ros_node::add_tcp_connection(static_cast<tcp_role>(request.role), request.port);

    return true;
}
bool ros_node::service_add_udp_connection(driver_modem_msgs::add_udp_connectionRequest& request, driver_modem_msgs::add_udp_connectionResponse& response)
{
    response.success = ros_node::add_udp_connection(request.port);

    return true;
}
bool ros_node::service_remove_connection(driver_modem_msgs::remove_connectionRequest& request, driver_modem_msgs::remove_connectionResponse& response)
{
    response.success = ros_node::remove_connection(static_cast<protocol>(request.protocol), request.port);

    return true;
}
bool ros_node::service_remove_all_connections(driver_modem_msgs::remove_all_connectionsRequest &request, driver_modem_msgs::remove_all_connectionsResponse &response)
{
    // Remove all connections.
    ros_node::remove_all_connections();

    // Set response to true.
    response.success = true;

    return true;
}
bool ros_node::service_tcp_tx(driver_modem_msgs::send_tcpRequest &request, driver_modem_msgs::send_tcpResponse &response, uint16_t port)
{
    response.success = ros_node::m_driver->tx(protocol::TCP, port, request.packet.data.data(), static_cast<uint32_t>(request.packet.data.size()));

    return true;
}
