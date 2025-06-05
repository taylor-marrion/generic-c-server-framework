/**
 * @file server_utils.h
 * @brief Server configuration, argument parsing, and connection setup utilities.
 * 
 * This module defines core utilities shared between multithreaded and select-based
 * TCP server implementations. It provides:
 * - Command-line argument parsing and config file loading
 * - Default configuration management with override support
 * - Validation of ports, file paths, log levels, and numeric values
 * - Server socket creation for both IPv4 and IPv6
 * - Thread-safe configuration structures for accept loops and client handlers
 * 
 * Designed for flexibility and reuse across multiple server models, this module
 * centralizes configuration logic and enforces consistent server setup behavior.
 */

#ifndef SERVER_UTILS_H
#define SERVER_UTILS_H

#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>


/* ========================= Macros and Constants ========================= */

#define MIN_PORT 1024   /**< Avoid well-known ports (0-1023) */
#define MAX_PORT 65535  /**< Maximum valid port number */
#define SERVER_DEFAULT_PORT 8000

#define MAX_LINE 512        /**< Maximum length of a line of text */
#define MAX_FILEPATH 256    /**< Maximum length of file path strings */

#define CONFIG_DIR "config/"    /**< Relative path to config directory */
#define SERVER_DEFAULT_CONFIG_FILE "server.conf" /**< Default config file path */

#define LOG_DIR "logs/"
#define SERVER_DEFAULT_LOG LOG_DIR"server.log"

#define DATA_DIR "data/"        /**< Relative path to data directory */
#define SERVER_DEFAULT_USER_FILE "users.db" /**< Default user DB filename */

#define SERVER_DEFAULT_LOG_LEVEL "INFO" 

#define NUM_WORKERS 8           /**< Default number of worker threads */
#define MAX_CLIENTS 8           /**< Max concurrent client threads */
#define BACKLOG 10              /**< Max pending connections in listen queue */


/* ========================= Struct Definitions ========================= */


/**
 * @struct server_config_t
 * @brief Configuration structure for initializing the server.
 *
 * Stores server configuration and socket-related settings such as 
 * port number, IP version, and transport layer protocol.
 */
typedef struct server_config_t {
    // --- Config Settings ---
    char source_path[MAX_FILEPATH]; /**< Path to the config file used */

    // --- Network Settings ---
    uint16_t port;           /**< Port number to listen on */
    uint16_t enable_ipv6;    /**< 0 for IPv4, 1 for IPv6 */
    uint16_t enable_udp;     /**< 0 for TCP, 1 for UDP */
    uint16_t max_clients;    /**< Maximum number of concurrent clients */
    uint16_t max_backlog;    /**< Maximum number of pending connections */
    uint16_t timeout_seconds;/**< Inactivity timeout for connections */

    // --- Logging Settings ---
    char log_level[16];             /**< Log level string (e.g., "DEBUG") */
    char log_file[MAX_FILEPATH];    /**< Path to output log file */
    bool log_to_stderr;             /**< Also log to stderr if true */
} server_config_t;


/**
 * @struct accept_loop_args_t
 * @brief Arguments passed to the main accept loop.
 *
 * Contains the server socket descriptor and will eventually include
 * shared data structures and mutexes for client handlers.
 *
 * Currently simplified for echo testing.
 */
typedef struct {
    int server_socket;  /**< Server socket descriptor */
} accept_loop_args_t;


/**
 * @struct client_handler_args_t
 * @brief Arguments passed to the client handler thread.
 *
 * Contains the server socket descriptor and will eventually include
 * shared data structures and mutexes for client handlers.
 *
 * Currently simplified for echo testing.
 */
typedef struct {
    int client_socket;  /**< Client socket descriptor */
} client_handler_args_t;


/* ========================= Function Prototypes ========================= */


/**
 * @brief Initializes server_config_t with default values.
 *
 * Sets safe, hardcoded defaults for all fields in the server configuration.
 *
 * @param config Pointer to the server_config_t to initialize.
 * @return true on success, false if input is NULL.
 */
bool init_default_config(server_config_t *config);


/**
 * @brief Validates that a string contains only allowed characters.
 *
 * Disallows:
 * - ASCII control characters (0x00–0x1F, 0x7F)
 * - Comma (,), double quote ("), single quote ('), and backslash (\)
 *
 * @param s The input string to validate.
 * @return 1 if valid, 0 otherwise.
 */
int is_valid_ascii_string(const char *s);


/**
 * @brief Loads server configuration from a key=value file.
 * 
 * Parses a simple configuration file (e.g., config/server.conf) containing 
 * key=value pairs. Values in the config file override current values in 
 * the `server_config_t` struct.
 * 
 * @note This is called BEFORE logging is initialized.
 * 
 * Supported keys:
 * - port=8000
 * - enable_ipv6=0|1
 * - enable_udp=0|1
 * - max_clients=N
 * - max_backlog=N
 * - timeout_seconds=N
 * - log_level=INFO|DEBUG|...
 * - log_file=logs/server.log
 * - log_to_stderr=true|false
 * 
 * @param config Pointer to the server_config_t to populate
 * @param file_path Path to the config file (e.g., config/server.conf)
 * @return 0 on success, -1 on failure (file not found or parse error)
 */
int load_server_config_from_file(server_config_t *config, const char *file_path);


/**
 * @brief Logs the parsed server configuration.
 * 
 * Prints the current server settings to the logging library,
 * including port number, IP version, and protocol mode.
 *
 * @param config Pointer to the server configuration struct.
 */
void log_server_config(const server_config_t *config);


/**
 * @brief Parses command-line arguments for the server application.
 * 
 * @note This is called BEFORE logging is initialized.
 *
 * @param argc Argument count from main().
 * @param argv Argument vector from main().
 * @param config Pointer to the server_config_t struct to populate.
 * @return 0 on success, -1 on failure.
 */
int parse_arguments_server(int argc, char *argv[], server_config_t *config);


/**
 * @brief Displays the usage information for the server application.
 *
 * This function prints detailed usage instructions for the server,
 * including descriptions of all supported command-line options.
 *
 * @param name Name of the executable, `argv[0]` from command-line arguments.
 */
void print_usage_server(char name[]);


/**
 * @brief Resolves the absolute path to a file in the project's config directory.
 *
 * This function computes the full path to a file in the relative `../config/` 
 * folder, based on the current location of the executable (`/proc/self/exe`). 
 * This allows the program to reliably access files like `server.conf` regardless 
 * of the working directory where the program was launched.
 *
 * @param[in] rel_path      Filename in the config/ directory (e.g., "server.conf").
 * @param[out] out_path     Buffer to store the full resolved path.
 * @param[in] max_len       Size of the output buffer.
 * @return 0 on success, -1 on failure.
 */
int resolve_config_path(const char *rel_path, char *out_path, size_t max_len);


/**
 * @brief Resolves the absolute path to a file in the project's data directory.
 *
 * This function computes the full path to a file in the relative `../data/` 
 * folder, based on the current location of the executable (`/proc/self/exe`). 
 * This allows the program to reliably access files like `users.db` regardless 
 * of the working directory where the program was launched.
 *
 * @param[in] rel_path      Filename in the data directory (e.g., "users.db").
 * @param[out] out_path     Buffer to store the full resolved path.
 * @param[in] max_len       Size of the output buffer.
 * @return 0 on success, -1 on failure.
 */
int resolve_data_path(const char *rel_path, char *out_path, size_t max_len);


/**
 * @brief Resolves client-supplied filename to an absolute path within data/.
 *
 * This function safely constructs a path by prepending the "data/" directory
 * to the provided filename. It ensures that the resolved path does not escape 
 * the intended data directory (i.e., via `..` or symbolic links). This protects
 * the server from unsafe file access outside the allowed storage area.
 *
 * @param input_filename The filename provided by the client (e.g., "dump.kvs").
 * @param resolved_path  Output buffer for the validated absolute path.
 * @param max_len        Size of the output buffer in bytes.
 * 
 * @return 0 on success, -1 on failure (e.g., invalid input or unsafe path).
 *
 * @note The caller must provide a buffer of at least PATH_MAX bytes.
 * @note The resolved path will always be NULL-terminated on success.
 */
int resolve_data_subpath(
    const char *input_filename, 
    char *resolved_path, 
    size_t max_len);


/**
 * @brief Initializes and returns a TCP server socket.
 *
 * Uses values from the given server_config_t structure to create and bind
 * a socket. If successful, returns the socket file descriptor.
 *
 * @param config Pointer to a valid server configuration struct.
 * @return Valid socket file descriptor on success, or -1 on error.
 */
int setup_server_socket(const server_config_t *config);


/**
 * @brief Validates and converts a string to an integer.
 *
 * Converts a string to an integer using strtol and ensures the value is within 
 * the specified range. If the input is invalid, an error message is printed and
 * the default value is returned.
 *
 * @param value The string to validate and convert.
 * @param min The minimum valid value.
 * @param max The maximum valid value.
 * @param default_value The value to return if the input is invalid.
 * @param flag_name The name of the flag being processed 
 *                  (used in error messages).
 * @return The validated integer, or the default value if the input is invalid.
 */
int validate_int(
    const char *value, 
    int min, 
    int max, 
    int default_value, 
    const char *flag_name);

#endif // SERVER_UTILS_H