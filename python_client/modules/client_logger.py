"""Logging setup for the key/value client.

Initializes a logging system that supports both file and stderr output,
with symbolic prefix formatting matching the C server macros.
"""

import logging
import os
from modules.client_utils import ClientConfig


# Symbolic prefix map: similar to C server logging
_SYMBOLIC_PREFIXES = {
    "DEBUG": "[~]",
    "INFO":  "[*]",
    "WARNING": "[!]",
    "ERROR": "[!]",
    "FATAL": "[x]",
}


class SymbolicFormatter(logging.Formatter):
    """Custom formatter that prepends symbolic log level indicators."""

    def format(self, record):
        symbol = _SYMBOLIC_PREFIXES.get(record.levelname, "[?]")
        record.msg = f"{symbol} {record.msg}"
        return super().format(record)


def setup_logger(config: ClientConfig) -> None:
    """Sets up the client logger based on the given config.

    Args:
        config: ClientConfig containing log_level, log_file, and log_to_stderr.
    """
    log_level = getattr(logging, config.log_level.upper(), logging.DEBUG)
    logger = logging.getLogger()
    logger.setLevel(log_level)
    logger.handlers.clear()

    formatter = SymbolicFormatter("%(message)s")

    if config.log_to_stderr:
        stream_handler = logging.StreamHandler()
        stream_handler.setFormatter(formatter)
        logger.addHandler(stream_handler)

    if config.log_file:
        os.makedirs(os.path.dirname(config.log_file), exist_ok=True)
        file_handler = logging.FileHandler(config.log_file, mode='a')
        file_handler.setFormatter(formatter)
        logger.addHandler(file_handler)
