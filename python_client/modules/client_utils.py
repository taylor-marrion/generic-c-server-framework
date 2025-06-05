"""Client configuration and argument parsing module.

Provides utilities for validating IP addresses and ports,
constructing a configuration object, and parsing CLI arguments.
"""

import argparse
import ipaddress
import sys
import re
import configparser
import os

from dataclasses import dataclass

# Constants
MIN_PORT = 1024
MAX_PORT = 65535
DEFAULT_PORT = 8000
DEFAULT_IP_ADDRESS = "127.0.0.1"
DEFAULT_CONFIG_FILE = "config/client.conf"


@dataclass
class ClientConfig:
    """
    Client configuration structure for connecting to the server.
    
    Attributes:
        ip_address (str): IP address to connect to.
        port (int): Port to connect to.
        enable_ipv6 (int): 0 for IPv4, 1 for IPv6.
        enable_udp (int): 0 for TCP, 1 for UDP.
    """
    server_ip: str = DEFAULT_IP_ADDRESS
    port: int = DEFAULT_PORT
    enable_ipv6: bool = False
    enable_udp: bool = False
    timeout_seconds: int = 60

    log_level: str = "DEBUG"
    log_file: str = "logs/client.log"
    log_to_stderr: bool = True

    script_path: str = None  # Optional path to a script file

    def __str__(self):
        proto = "UDP" if self.enable_udp else "TCP"
        version = "IPv6" if self.enable_ipv6 else "IPv4"
        return (
            f"========= Client Configuration =========\n"
            f"    Server IP:   {self.server_ip}\n"
            f"    Port:        {self.port}\n"
            f"    IP Mode:     {version}\n"
            f"    Protocol:    {proto}\n"
            f"    Timeout:     {self.timeout_seconds} sec\n"
            f"    Log File:    {self.log_file}\n"
            f"    Log to STDERR: {self.log_to_stderr}\n"
            f"    Log Level:   {self.log_level}\n"
            f"    Script:      {self.script_path}\n"
            f"========================================="
        )


def parse_arguments_into_config() -> ClientConfig:
    """Parses command-line arguments and returns a ClientConfig object.

    Returns:
        ClientConfig: Parsed command-line arguments.
    """
    parser = argparse.ArgumentParser(description="Client Config Parser")
    parser.add_argument(
        "-i", "--ip_address", type=validate_ip, help="Server IP address."
    )
    parser.add_argument(
        "-p", "--port", type=validate_port, help="Server port number."
    )
    parser.add_argument(
        "-s", "--script", type=str,
        help="Path to a command script file (non-interactive mode)."
    )
    parser.add_argument(
        "-c", "--config", type=str, default=DEFAULT_CONFIG_FILE,
        help="Path to client config file (default: client.conf)."
    )

    args = parser.parse_args()
    client_config = load_config_file(args.config)

    # Override with CLI arguments (if provided)
    if args.ip_address:
        client_config.server_ip = args.ip_address
    if args.port:
        client_config.port = args.port
    if args.script:
        client_config.script_path = args.script

    return client_config


def load_config_file(path: str) -> ClientConfig:
    """Loads client configuration from an INI-style config file."""
    cfg = configparser.ConfigParser()
    if not os.path.exists(path):
        print(f"Warning: Config file '{path}' not found. Using defaults.")
        return ClientConfig()

    cfg.read(path)

    # Helper to safely get and convert values
    def get(section, option, fallback, conv=str):
        try:
            return conv(cfg.get(section, option))
        except Exception:
            return fallback

    return ClientConfig(
        server_ip=get("Client Configuration", "server_ip", DEFAULT_IP_ADDRESS),
        port=get("Client Configuration", "server_port", DEFAULT_PORT, int),
        enable_ipv6=get("Client Configuration", "use_ipv6", False, lambda x: x.lower() == "true"),
        enable_udp=get("Client Configuration", "use_udp", False, lambda x: x.lower() == "true"),
        timeout_seconds=get("Client Configuration", "timeout_seconds", 10, int),
        log_level=get("Logging Configuration", "log_level", "DEBUG"),
        log_file=get("Logging Configuration", "log_file", "logs/client.log"),
        log_to_stderr=get("Logging Configuration", "log_to_stderr", True, lambda x: x.lower() == "true")
    )


def check_version(major: int, minor: int) -> bool:
    """
    Check if the current Python version meets the minimum required version.

    Args:
        major (int): The required major Python version.
        minor (int): The required minor Python version.

    Returns:
        bool: True if the system's Python version is equal to or newer than
        the required version, False otherwise.

    Raises:
        None
    """
    if sys.version_info[0] < major:
        print(f"Error! This program requires Python {major}.{minor} or newer.")
        return False

    if(sys.version_info[0] == major and sys.version_info[1] < minor):
        print(f"Error! This program requires Python {major}.{minor} or newer.")
        return False

    print("This system meets Python version requirements to run.")
    return True


def input_number(message: str) -> int:
    """
    Prompts the user for an integer input.

    Args:
        message (str): The prompt message displayed to the user.

    Returns:
        int: The validated integer entered by the user.

    Raises:
        None
    """
    while True:
        try:
            user_input = int(input(message))
            return user_input
        except ValueError:
            print("Not an integer! Try again.")


def validate_ip(ip_value: str) -> str:
    """
    Validate the given IP address.

    Args:
        ip_value (str): The IP address to validate.

    Returns:
        str: The validated IP address.

    Raises:
        argparse.ArgumentTypeError: If the IP address is invalid.
    """
    try:
        ipaddress.ip_address(ip_value)
        return ip_value
    except ValueError as ex:
        raise argparse.ArgumentTypeError(
            f"Invalid IP address: {ip_value}"
        ) from ex


def validate_port(port: str, default_port: int = DEFAULT_PORT) -> int:
    """
    Validate the given port number.

    Args:
        port (int or str): The port number to validate.
        default_port (int, optional): The default port to use if the given
        port is invalid or out of range. Defaults to 8000.

    Returns:
        int: The validated port number.

    Raises:
        argparse.ArgumentTypeError: If the port is not a number or out of range.
    """
    try:
        port = int(port)
        if not (MIN_PORT <= port <= MAX_PORT):
            print(
                f"Error: Port {port} is out of the valid range "
                f"({MIN_PORT}-{MAX_PORT}).")
            print(f"Using default value of {default_port}.")
            return default_port
        return port
    except ValueError as ex:
        raise argparse.ArgumentTypeError(f"Invalid port number: {port}") from ex

def is_valid_string(s: str) -> bool:
    """
    Validates that the input string does not contain disallowed characters.

    Disallowed characters include:
        - ASCII control characters: \x00–\x1F and \x7F
        - Comma (,), double quote ("), single quote ('), and backslash (\\)

    These are excluded to ensure compatibility with CSV storage, text parsing,
    and command-line safety.

    Args:
        s (str): The input string to validate.

    Returns:
        bool: True if the string is valid (contains only safe characters),
              False otherwise.
    """
    return bool(re.fullmatch(r"[^\x00-\x1F\x7F,\"\\']+", s))
