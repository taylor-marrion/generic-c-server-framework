/**
 * @file net_lib.h
 * @brief Public API for the net_lib library.
 * 
 * Provides an aggregation of utility functions and structures for network 
 * management and communication.
 */

#ifndef NET_LIB_H
#define NET_LIB_H


#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>      // Required for struct addrinfo
#include <unistd.h>
#include <stddef.h>
#include <sys/select.h> // Required for fd_set


#define DEFAULT_TIMEOUT_SEC 5   /**< Default socket timeout in seconds */


/** Forward declaration for addrinfo */
struct addrinfo;


/**
 * @brief Represents a single connection on the server.
 *
 * This structure is used to store information about a client connection,
 * including the socket file descriptor, address, and a unique identifier.
 */
typedef struct client_connection_t {
    int32_t socket_fd;                  /**< Socket file descriptor */
    struct sockaddr_storage address;    /**< IPv4 or IPv6 client address */
    socklen_t addr_len;                 /**< Length of the address */
    int32_t active;                     /**< 1 if active, 0 if inactive */
    uint32_t id;                        /**< Optional unique connection ID */
} client_connection_t;


/**
 * @brief Initializes a server socket to listen for incoming connections.
 *
 * Dynamically supports IPv4/IPv6 and TCP/UDP based on the configuration.
 * The caller specifies the maximum number of queued connections using 
 * the `backlog` parameter.
 *
 * @param port The port to bind the socket to (e.g., "443" or "https").
 * @param enable_ipv6 Use IPv6 if non-zero, otherwise use IPv4.
 * @param enable_udp Use UDP if non-zero, otherwise use TCP.
 * @param backlog Max queued connections for TCP (ignored for UDP).
 * @return File descriptor for the initialized server socket, or -1 on error.
 */
int initialize_server_socket(
    const char *port, 
    int enable_ipv6, 
    int enable_udp, 
    int backlog);


/**
 * @brief Creates and binds a socket to the specified address.
 *
 * Handles both IPv4 and IPv6 addresses and protocol configuration.
 *
 * @param addrinfo The address information structure returned by getaddrinfo.
 * @param enable_ipv6 Use IPv6 if non-zero, otherwise use IPv4.
 * @return The file descriptor for the created and bound socket, or -1 on error.
 */
int create_and_bind_socket(struct addrinfo *addr, int enable_ipv6);


/**
 * @brief Placeholder for handling incoming data from a connected client.
 *
 * This function is used for testing basic server functionality. It reads data
 * from a client socket and echoes the received data back to the client. If the
 * client disconnects or an error occurs, the function returns 0 to signal the
 * disconnection. This function does not handle project-specific logic and is
 * intended to be replaced by more detailed request-handling functionality.
 *
 * @param client_socket The file descriptor for the client's socket.
 * @return 1 if client is still connected, 0 if disconnected or error occurs.
 *
 * @note Replace this function with a project-specific handler for processing
 * structured client requests and responses.
 */
int echo_client_message(int client_socket);


/**
 * @brief Extracts a printable IP address from a client_connection_t.
 *
 * @param client Pointer to a client_connection_t.
 * @param buffer Output buffer to store the string IP address.
 * @param buflen Length of the output buffer.
 * @return 0 on success, -1 on failure.
 */
int get_client_ip_string(
    const client_connection_t *client, 
    char *buffer, 
    size_t buflen);


/**
 * @brief Handles a new client connection on the server socket.
 *
 * Accepts a new connection from the server's listening socket and adds the
 * new client to the `connections` array. Updates the master file descriptor set
 * and the maximum file descriptor if necessary.
 * 
 * @param master_set Set of active file descriptors (updated on new connection).
 * @param max_fd Pointer to current maximum file descriptor (may be updated).
 * @param server_socket The server's listening socket file descriptor.
 * @param active_connections Pointer to count of active clients (incremented).
 * @return 0 on success, -1 if the connection could not be accepted or added.
 */
void handle_new_connection(
    fd_set *master_set, 
    int *max_fd, 
    int server_socket, 
    int *active_connections);


/**
 * @brief Receives the full expected amount of data from a socket.
 *
 * This function ensures that exactly `expected_bytes` are read into `buffer`
 * from the given socket. It repeatedly calls `recv()` until all data is
 * received, the connection is closed, or an error occurs.
 *
 * @param socket_fd The socket file descriptor.
 * @param buffer The buffer to store received data.
 * @param expected_bytes The total number of bytes expected to be received.
 * 
 * @return The total number of bytes received, or:
 *         - `0` if the connection was closed before all data was received.
 *         - `-1` if an error occurred (logs to `stderr`).
 */
ssize_t recv_all(int socket_fd, unsigned char *buffer, size_t expected_bytes);


/**
 * @brief Receives all expected bytes from a socket, with optional timeout.
 *
 * This version uses select() internally to wait for socket readiness.
 *
 * @param socket_fd The socket descriptor.
 * @param buffer Buffer to store received data.
 * @param expected_bytes Number of bytes to receive.
 * @param timeout_sec Timeout in seconds (-1 for infinite wait).
 * 
 * @return Number of bytes received, or:
 *         - `0` if timeout or disconnect.
 *         - `-1` on error.
 */
ssize_t recv_all_with_timeout(
    int socket_fd, 
    unsigned char *buffer, 
    size_t expected_bytes, 
    int timeout_sec);


/**
 * @brief Sends all data through a socket, ensuring full transmission.
 *
 * This function repeatedly calls `send()` until all `length` bytes from 
 * `buffer` are sent. It ensures that partial sends are handled correctly and 
 * retries if interrupted by a signal (`EINTR`). If the connection is closed 
 * unexpectedly, the function returns an error.
 *
 * @param socket_fd The socket file descriptor.
 * @param buffer The buffer containing the data to send.
 * @param length The total number of bytes to send.
 * 
 * @return The total number of bytes sent, or:
 *         - `-1` if an error occurred (logs to `stderr`).
 */
ssize_t send_all(int socket_fd, const unsigned char *buffer, size_t length);


/**
 * @brief Sends all bytes through a socket, with optional timeout.
 *
 * This version uses select() internally to wait for socket writability.
 *
 * @param socket_fd The socket descriptor.
 * @param buffer Buffer with data to send.
 * @param length Number of bytes to send.
 * @param timeout_sec Timeout in seconds (-1 for infinite wait).
 * 
 * @return Number of bytes sent, or:
 *         - `-1` on error or timeout.
 */
ssize_t send_all_with_timeout(
    int socket_fd, 
    const unsigned char *buffer, 
    size_t length, 
    int timeout_sec);


#endif // NET_LIB_H