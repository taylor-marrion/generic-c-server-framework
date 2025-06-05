/**
 * @file net_lib.c
 * @brief Implementation of networking utilities for server-client communication.
 * 
 * This module provides a reusable library of socket utility functions for 
 * IPv4/IPv6 and TCP/UDP network communication. It supports socket creation, 
 * binding, listening, connection handling, full-duplex send/receive with 
 * timeout, and IP address extraction.
 * 
 * Designed for use in both multithreaded and select()-based server architectures.
 *
 * Features:
 * - Dual-stack IPv4/IPv6 support
 * - TCP and UDP support
 * - Full data transmission with `send_all()` / `recv_all()`
 * - Select-based timeout handling for robustness
 * - Logging integration via `log_lib`
 * 
 * Heavily influenced by Beej's Guide to Network Programming:
 * https://beej.us/guide/bgnet/
 *
 */

#define _POSIX_C_SOURCE 200112L // Required: POSIX compatibility for getaddrinfo

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netdb.h>

#include "../include/log_lib.h"
#include "../include/net_lib.h"


int initialize_server_socket(
    const char *port, 
    int enable_ipv6, 
    int enable_udp, 
    int backlog) 
{
    int socket_fd;
    struct addrinfo hints;
    struct addrinfo * servinfo;
    struct addrinfo * p;
    //int yes = 1;

    // Step 1: Prepare hints structure for getaddrinfo
    memset(&hints, 0, sizeof hints);
    hints.ai_family = enable_ipv6 ? AF_INET6 : AF_INET;         // IPv4 or IPv6
    hints.ai_socktype = enable_udp ? SOCK_DGRAM : SOCK_STREAM;  // UDP or TCP
    hints.ai_flags = AI_PASSIVE;                    // Use any local address

    // Step 2: Get address information
    if (getaddrinfo(NULL, port, &hints, &servinfo) != 0) 
    {
        LOG_ERROR("initialize_server_socket: %s", strerror(errno));
        return -1;
    }

    // Step 3: Loop through the results and bind to the first valid address
    for (p = servinfo; p != NULL; p = p->ai_next) 
    {
       socket_fd = create_and_bind_socket(p, enable_ipv6);
       if (socket_fd != -1) 
        {
            break; // Successfully bound
        }
    }

    // Step 4: Check if binding was successful
    if (p == NULL) 
    {
        LOG_FATAL("initialize_server_socket: Failed to bind to any address.");
        freeaddrinfo(servinfo);
        return -1;
    }

    // Step 5: Free the address information structure
    freeaddrinfo(servinfo);  // Done with address info

    // Step 6: If using TCP, start listening for incoming connections
    if (!enable_udp && listen(socket_fd, backlog) == -1) 
    {
        LOG_ERROR("initialize_server_socket: %s", strerror(errno));
        close(socket_fd);
        return -1;
    }

    // Step 7: Print success message
    LOG_CREATE("Server is listening on port %s (%s/%s)\n", port,
           enable_ipv6 ? "IPv6" : "IPv4", enable_udp ? "UDP" : "TCP");

    return socket_fd;
} // end initialize_server_socket


int create_and_bind_socket(struct addrinfo *addr, int enable_ipv6)
{
    int socket_fd;
    int yes = 1;

    // Step 3a: Create socket
    socket_fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (socket_fd == -1) 
    {
        LOG_ERROR("create_and_bind_socket socket: %s\n", strerror(errno));
        return -1;
    }

    // Step 3b: Allow port reuse
    if (setsockopt(
        socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
    {
        LOG_ERROR("create_and_bind_socket setsockopt: %s\n", strerror(errno));
        close(socket_fd);
        return -1;
    }

    // Step 3c: Disable IPV6_V6ONLY for dual-stack support (if IPv6 is enabled)
    if (enable_ipv6 && addr->ai_family == AF_INET6) 
    {
        int dual_stack = 0; // 0 = allow both IPv4 and IPv6
        if (setsockopt(socket_fd, IPPROTO_IPV6, IPV6_V6ONLY, 
                        &dual_stack, sizeof(dual_stack)) == -1) 
        {
            LOG_ERROR("create_and_bind_socket setsockopt IPV6_V6ONLY: %s\n", 
                    strerror(errno));
            close(socket_fd);
            return -1;
        }
    }

    // Step 3d: Bind socket to the address
    if (bind(socket_fd, addr->ai_addr, addr->ai_addrlen) == -1) 
    {
        LOG_ERROR("create_and_bind_socket bind: %s\n", strerror(errno));
        close(socket_fd);
        return -1;
    }

    return socket_fd; // Successfully created and bound socket
} // end create_and_bind_socket


int echo_client_message(int client_socket) 
{
    char buffer[1024];
    int bytes_read = recv(client_socket, buffer, sizeof(buffer), 0);

    if (bytes_read <= 0) 
    {
        if (bytes_read == 0)
        {
            LOG_RECV("Client [%d] disconnected.", client_socket);
        }
        else
        {
            LOG_ERROR("recv failed on client [%d]: %s",
                      client_socket, strerror(errno));
        }
        return 0;
    }

    buffer[bytes_read] = '\0';
    LOG_RECV("Client [%d]: %s", client_socket, buffer);

    // Echo the message back to the client
    ssize_t bytes_sent = send(client_socket, buffer, (size_t)bytes_read, 0);
    if (bytes_sent != bytes_read)
    {
        LOG_WARNING("Partial/failed send to client [%d]: %zd of %zd bytes.",
                    client_socket, bytes_sent, bytes_read);
        return 0;
    }

    LOG_SEND("Echoed message back to client [%d] (%zd bytes).",
             client_socket, bytes_sent);

    return 1;
} // end echo_client_message


int get_client_ip_string(
    const client_connection_t *client, 
    char *buffer, 
    size_t buflen)
{
    if (client == NULL || buffer == NULL || buflen == 0)
    {
        LOG_ERROR("get_client_ip_string: Invalid argument(s).");
        return -1;
    }

    void *addr_ptr = NULL;

    if (client->address.ss_family == AF_INET) 
    {
        addr_ptr = &((struct sockaddr_in *)&client->address)->sin_addr;
    } 
    else if (client->address.ss_family == AF_INET6) 
    {
        addr_ptr = &((struct sockaddr_in6 *)&client->address)->sin6_addr;
    } 
    else 
    {
        LOG_WARNING("get_client_ip_string: Unsupported address family (%d).",
                    client->address.ss_family);
        return -1;
    }

    if (!inet_ntop(client->address.ss_family, addr_ptr, buffer, buflen)) 
    {
        LOG_ERROR("get_client_ip_string: inet_ntop failed: %s",
                  strerror(errno));
        return -1;
    }

    LOG_DEBUG("get_client_ip_string: Parsed client IP as '%s'.", buffer);
    return 0;
} // end get_client_ip_string


void handle_new_connection(
    fd_set *master_set, 
    int *max_fd, 
    int server_socket, 
    int *active_connections) 
{
    if (!master_set || !max_fd || !active_connections)
    {
        LOG_ERROR("handle_new_connection: Null argument(s).");
        return;
    }

    struct sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_socket = accept(
            server_socket, (struct sockaddr*)&client_addr, &addr_len);
    if (client_socket == -1) 
    {
        LOG_ERROR("accept failed: %s", strerror(errno));
        return;
    }

    // Add the client socket to the master set
    FD_SET(client_socket, master_set);

    // Update max_fd
    if (client_socket > *max_fd) 
    {
        *max_fd = client_socket;
    }

    (*active_connections)++;

    char ip_str[INET6_ADDRSTRLEN];
    client_connection_t tmp_client = {
        .address = client_addr,
        .addr_len = addr_len
    };
    get_client_ip_string(&tmp_client, ip_str, sizeof(ip_str));

    LOG_INFO("New client connected from %s (fd = %d)", ip_str, client_socket);
} // end handle_new_connection


ssize_t recv_all(int socket_fd, unsigned char *buffer, size_t expected_bytes) 
{
    return recv_all_with_timeout(
        socket_fd, buffer, expected_bytes, DEFAULT_TIMEOUT_SEC);
} // end recv_all


ssize_t recv_all_with_timeout(
    int socket_fd, 
    unsigned char *buffer, 
    size_t expected_bytes, 
    int timeout_sec)
{
    size_t total_received = 0;
    ssize_t bytes_received;

    while (total_received < expected_bytes) 
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(socket_fd, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = timeout_sec;
        timeout.tv_usec = 0;

        int ready = select(socket_fd + 1, &read_fds, NULL, NULL,
                          (timeout_sec >= 0) ? &timeout : NULL);

        if (ready == -1) 
        {
            LOG_ERROR(
                "recv_all_with_timeout: select failed (fd=%d): %s",
                socket_fd, strerror(errno));
            return -1;
        } 
        else if (ready == 0) 
        {
            LOG_WARNING(
                "recv_all_with_timeout: timed out after %d sec (fd=%d)",
                timeout_sec, socket_fd);
            return 0;
        }

        bytes_received = recv(
            socket_fd, buffer + total_received, 
            expected_bytes - total_received, 0);

        if (bytes_received < 0) 
        {
            LOG_ERROR(
                "recv_all_with_timeout: recv failed (fd=%d): %s",
                socket_fd, strerror(errno));
            return -1;
        }

        if (bytes_received == 0) 
        {
            LOG_WARNING(
                "recv_all_with_timeout: connection closed (fd=%d)", 
                socket_fd);
            return 0;
        }

        total_received += bytes_received;
    }

    return total_received;
} // end recv_all_with_timeout


ssize_t send_all(int socket_fd, const unsigned char *buffer, size_t length) 
{
    return send_all_with_timeout(
        socket_fd, buffer, length, DEFAULT_TIMEOUT_SEC);
} // end send_all


ssize_t send_all_with_timeout(
    int socket_fd, 
    const unsigned char *buffer, 
    size_t length, 
    int timeout_sec)
{
    size_t total_sent = 0;

    while (total_sent < length) 
    {
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(socket_fd, &write_fds);

        struct timeval timeout;
        timeout.tv_sec = timeout_sec;
        timeout.tv_usec = 0;

        int ready = select(socket_fd + 1, NULL, &write_fds, NULL,
                          (timeout_sec >= 0) ? &timeout : NULL);

        if (ready == -1) 
        {
            LOG_ERROR(
                "send_all_with_timeout: select failed (fd=%d): %s", 
                socket_fd, strerror(errno));
            return -1;
        } 
        else if (ready == 0) 
        {
            LOG_WARNING(
                "send_all_with_timeout: timed out after %d sec (fd=%d)", 
                timeout_sec, socket_fd);
            return -1;
        }

        ssize_t sent = send(
            socket_fd, buffer + total_sent, length - total_sent, 0);

        if (sent < 0) 
        {
            if (errno == EINTR) 
            {
                continue;
            }

            LOG_ERROR(
                "send_all_with_timeout: send failed (fd=%d): %s", 
                socket_fd, strerror(errno));
            return -1;
        }

        if (sent == 0) 
        {
            LOG_WARNING(
                "send_all_with_timeout: connection closed (fd=%d)", 
                socket_fd);
            return -1;
        }

        LOG_SEND("send_all_with_timeout: sent %zd bytes (fd=%d)", 
                sent, socket_fd);

        total_sent += sent;
    }

    return total_sent;
} // end send_all_with_timeout
