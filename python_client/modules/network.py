"""Handles networking functionality for the key/value store client.

Includes TCP/UDP socket creation, connection establishment, and buffer
receiving utilities used in the client-side communication with the server.
"""

import socket
import sys
import errno


def setup_connection(
        ip_address: str, port: int, enable_udp: bool, enable_ipv6: bool
        ) -> socket.socket:
    """Creates and returns a socket connection based on provided settings.

    Args:
        ip_address (str): The server IP address.
        port (int): The server port number.
        enable_udp (bool): True for UDP, False for TCP.
        enable_ipv6 (bool): True for IPv6, False for IPv4.

    Returns:
        socket.socket: A configured client socket.

    Raises:
        SystemExit: If the socket fails to initialize.
    """
    try:
        # Determine address family and socket type
        address_family = socket.AF_INET6 if enable_ipv6 else socket.AF_INET
        socket_type = socket.SOCK_DGRAM if enable_udp else socket.SOCK_STREAM

        # Create the socket
        client_socket = socket.socket(address_family, socket_type)

        if enable_udp:
            print(f"UDP socket created for {ip_address}:{port}.")
            return client_socket  # No need to connect for UDP

        # Establish TCP connection
        print(f"Connecting to {ip_address}:{port} over "
              f"{'IPv6' if enable_ipv6 else 'IPv4'}...")
        client_socket.connect((ip_address, port))
        print("Connection established.")

        return client_socket

    except socket.error as ex:
        print(f"Error: Could not establish connection to "
              f"{ip_address}:{port} ({ex})")
        sys.exit(1)  # Exit with error status


def test_message(client_socket: socket.socket) -> str:
    """Sends a test message to the server and prints the response.

    Args:
        client_socket (socket.socket): The active client socket.
    
    Returns:
        str: The decoded server response, or an error message if failed.
    """
    try:
        message = "Hello, Server!"
        print(f"Sending message: {message}")
        client_socket.sendall(message.encode())

        response = client_socket.recv(1024).decode()
        print(f"Server response: {response}")
        return response

    except socket.error as ex:
        err_str = f"Error: Failed to communicate with server ({ex})"
        print(err_str)
        return err_str


def recvall(sock: socket.socket, expected_bytes: int) -> bytes:
    """
    Ensures the entire message of the given length is received.

    Args:
        sock (socket.socket): The socket to receive data from.
        expected_bytes (int): Number of bytes expected.

    Returns:
        bytes: The complete received data,
                or an empty bytes object if an error occurs.
    """
    buffer = bytearray()
    total_received = 0

    while total_received < expected_bytes:
        try:
            chunk = sock.recv(expected_bytes - total_received)
            if not chunk:
                print(f"Error: Connection closed while receiving. Received "
                      f"{total_received}/{expected_bytes} bytes.")
                return b''

            buffer.extend(chunk)
            total_received += len(chunk)

        except socket.error as e:
            print(f"Error: recvall() expected {expected_bytes} bytes but only "
                  f"received {total_received} bytes! Exception: {e}")
            return b''

    return bytes(buffer)

def is_socket_closed(sock: socket.socket) -> bool:
    """Checks if the socket is closed by attempting to peek at data."""
    try:
        data = sock.recv(1, socket.MSG_PEEK)
        if not data:
            return True
    except socket.error as e:
        if e.errno == errno.ECONNRESET:
            return True
    return False
