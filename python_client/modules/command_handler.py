"""Interactive client command session for the generic client-server framework.

This module provides an interactive loop for sending user input to the server
and printing server responses. It handles:

- Prompting for input
- Sending raw messages to the server
- Receiving and printing server responses
- Handling 'exit', 'quit', and 'help' commands
- Logging all interactions

This minimal implementation is designed for echo servers and future expansion.
"""

import logging
import socket
import sys
import select

from modules.user_session import UserSession


def interactive_client_session(client_socket: socket.socket, user: UserSession, timeout_seconds: int) -> None:
    """Launches a generic interactive session.

    Prompts the user for input, sends each line to the server,
    and prints the echoed response.

    Args:
        client_socket (socket.socket): Active TCP socket connected to server.
        user (UserSession): Current user session object.
    """
    logging.info("Connected to server. Type 'help' for available commands.")
    logging.info("Type 'exit' or 'quit' or press Ctrl+C to quit.")

    try:
        while True:
            prompt = f"Client ({user.username})> "
            user_input = input_with_timeout(prompt, timeout_seconds)
            
            if user_input is None:
                logging.warning("Idle timeout: No user input.")
                break  # Disconnect client

            if user_input == "":
                continue  # User pressed Enter without typing

            if user_input.lower() in ("exit", "quit"):
                logging.info("User requested exit.")
                break

            if user_input.lower() == "help":
                display_help()
                continue

            # Send message to server
            try:
                client_socket.sendall(user_input.encode())
                logging.debug("[>] Sent: %s", user_input)
            except socket.error as err:
                logging.error("Send failed: %s", err)
                break

            # Receive response from server
            try:
                response = client_socket.recv(1024)
                if not response:
                    logging.warning("Server closed the connection.")
                    break
                logging.debug("[<] Received: %s", response.decode())
                print(f"Server> {response.decode()}")
                print("")

            except socket.timeout:
                logging.warning("No response from server (timeout). Continuing...")
                continue
            except socket.error as err:
                logging.error("Receive failed: %s", err)
                break

    except KeyboardInterrupt:
        logging.info("Session interrupted by user.")

    logging.info("Closing session.")


def input_with_timeout(prompt: str, timeout: int) -> str | None:
    """Prompt user for input with a timeout."""
    print(prompt, end='', flush=True)
    ready, _, _ = select.select([sys.stdin], [], [], timeout)
    if ready:
        return sys.stdin.readline().strip()
    return None


def display_help() -> None:
    """Displays a list of available commands with usage descriptions."""
    print("\nAvailable commands:")
    print("  help        Show this help message")
    print("  exit, quit  Terminate the client session")
    print("  <message>   Any other input is sent to the server")
    print("")
