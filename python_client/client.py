#!/usr/bin/env python3
# requires Python 3.10 or newer

# Linux systems run: sed -i -e 's/\r$//' client.py
# removes "CR LF" from Windows text editors

"""Generic TCP client for the generic C server framework.

Handles:
- Command-line argument parsing (IP, port, and protocol options)
- Configuration validation
- Establishing a TCP socket connection
- Logging setup based on client.conf
- Graceful error handling and session teardown
- Future support for interactive client commands

Note:
    This client defaults to IPv4 and TCP only.
"""

import logging
import socket
import sys

from modules import client_utils, command_handler
from modules.user_session import UserSession
from modules.network import setup_connection
from modules.client_logger import setup_logger


def main():
    """Main entry point for the client.

    Parses CLI arguments and config file, sets up logging, establishes a
    connection, and launches an interactive client session. Gracefully
    handles socket errors and user interruption.
    """
    # check for valid Python version
    if client_utils.check_version(3, 10) is False:
        sys.exit(1)

    client_config = client_utils.parse_arguments_into_config()
    
    # Initialize logging from config
    setup_logger(client_config)

    logging.debug("Parsed client configuration:\n%s", client_config)

    # Initialize User BEFORE the interactive session
    user = UserSession()  # Default to guest user

    #initialize_command_registry()
    #initialize_response_registry()

    client_socket = None

    # Set up the client connection
    try:
        logging.info("Creating socket...")
        client_socket = setup_connection(
            client_config.server_ip,
            client_config.port,
            client_config.enable_udp,
            client_config.enable_ipv6
        )

        client_socket.settimeout(client_config.timeout_seconds)

        if client_config.enable_udp:
            #<<<Placeholder for UDP logic>>>
            logging.warning("UDP mode is currently unsupported.")
            logging.warning("How did you even do that? Exiting.")
            client_socket.close()
            sys.exit(0) # successful exit
        else:
            # Connect to server (TCP)
            command_handler.interactive_client_session(client_socket, user, client_config.timeout_seconds)


    except socket.error as ex:
        print(f"Socket error: {ex}")

    except KeyboardInterrupt:
        print("\nUser interrupted. Exiting client...")

    finally:
        # Clean up
        if client_socket and not client_config.enable_udp:
            client_socket.close()

    logging.info("Client shutdown complete.")
    sys.exit(0) # successful exit


if __name__ == "__main__":
    main()
