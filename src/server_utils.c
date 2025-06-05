/**
 * @file server_utils.c
 * @brief Server configuration and argument parsing implementation.
 * 
 * This file contains helper functions used by the server application to:
 * - Command-line argument parsing
 * - Port and address validation
 * - Server socket setup
 * - Help/usage output
 */

#define _POSIX_C_SOURCE 200112L // Required for readlink()

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>     // for readlink()
#include <getopt.h>
#include <limits.h>
#include <netinet/in.h> // for INET6_ADDRSTRLEN
#include <libgen.h>     // for dirname()

#include "../include/server_utils.h"

#include "../include/log_lib.h"
#include "../include/net_lib.h"

// Forward declaration (for portability)
char *realpath(const char *restrict path, char *restrict resolved_path);

/**
 * @brief Supported command-line options for the server application.
 *
 * This array defines the long options supported by the server.
 * Each entry corresponds to a specific command-line option.
 * Each option can be provided as a long or short flag.
 *
 * | Long Option     | Short | Argument Type | Description              |
 * |-----------------|-------|---------------|--------------------------|
 * | `--config`      | `-c`  | Required      | Path to server config file |
 * | `--port`        | `-p`  | Required      | Port to listen on (default: 8000) |
 * | `--help`        | `-h`  | None          | Print usage information and exit  |
 */
static struct option long_options_server[] = 
{
    {"config", required_argument, 0, 'c'}, // Config file path
    {"port", required_argument, 0, 'p'},            // Listening port
    {"help", no_argument, 0, 'h'},                  // Display help
    {0, 0, 0, 0}
};

/**
 * @brief Initializes server_config_t with default values.
 *
 * Sets safe, hardcoded defaults for all fields in the server configuration.
 *
 * @param config Pointer to the server_config_t to initialize.
 * @return true on success, false if input is NULL.
 */
bool init_default_config(server_config_t *config)
{
    if (config == NULL)
        return false;

    memset(config, 0, sizeof(server_config_t));

    // --- Network Defaults ---
    config->port             = SERVER_DEFAULT_PORT;
    config->enable_ipv6      = 0;
    config->enable_udp       = 0;
    config->max_clients      = MAX_CLIENTS;
    config->max_backlog      = BACKLOG;
    config->timeout_seconds  = 10;

    // --- Logging Defaults ---
    strncpy(config->log_level, SERVER_DEFAULT_LOG_LEVEL, sizeof(config->log_level) - 1);
    config->log_level[sizeof(config->log_level) - 1] = '\0'; // null-terminate

    strncpy(config->log_file, SERVER_DEFAULT_LOG, sizeof(config->log_file) - 1);
    config->log_file[sizeof(config->log_file) - 1] = '\0';

    config->log_to_stderr = true;

    return true;
} // end init_default_config


/**
 * @brief Validates that a string contains only allowed characters.
 *
 * @param s The input string to validate.
 * @return 1 if valid, 0 otherwise.
 */
int is_valid_ascii_string(const char *s)
{
    if (s == NULL)
    {
        return 0;
    }

    while (*s)
    {
        unsigned char c = (unsigned char)*s;
        if ((c <= 0x1F || c == 0x7F) || 
            c == ',' || c == '"' || c == '\'' || c == '\\')
            return 0;
        s++;
    }
    return 1;
} // end is_valid_ascii_string


/**
 * @brief Helper function: Trim leading/trailing whitespace from a string.
 */
static void trim(char *s)
{
    if (!s) return;

    // Left trim
    while (isspace((unsigned char)*s)) s++;

    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
} // end trim


/**
 * @brief Loads server configuration from a key=value file.
 * 
 * @note This is called BEFORE logging is initialized.
 * 
 * @param file_path Path to the config file (e.g., config/server.conf)
 * @param config Pointer to the server_config_t to populate
 * @return 0 on success, -1 on failure (file not found or parse error)
 */
int load_server_config_from_file(server_config_t *config, const char *file_path)
{
    if (!config || !file_path)
    {
        fprintf(stderr, "[!] load_server_config_from_file: NULL input.\n");
        return -1;
    }

    char resolved_path[MAX_FILEPATH];
    if (resolve_config_path(file_path, resolved_path, sizeof(resolved_path)) < 0)
    {
        fprintf(stderr, "[!] Failed to resolve config path: %s\n", file_path);
        return -1;
    }

    FILE *fp = fopen(resolved_path, "r");
    if (!fp)
    {
        fprintf(stderr, "[!] Could not open config file: %s\n", resolved_path);
        return -1;
    }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp))
    {
        // Skip comments and blank lines
        if (line[0] == '#' || line[0] == '\n') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = line;
        char *value = eq + 1;

        trim(key);
        trim(value);

        if (strcmp(key, "port") == 0)
            config->port = (uint16_t)atoi(value);
        else if (strcmp(key, "enable_ipv6") == 0)
            config->enable_ipv6 = (uint16_t)atoi(value);
        else if (strcmp(key, "enable_udp") == 0)
            config->enable_udp = (uint16_t)atoi(value);
        else if (strcmp(key, "max_clients") == 0)
            config->max_clients = (uint16_t)atoi(value);
        else if (strcmp(key, "max_backlog") == 0)
            config->max_backlog = (uint16_t)atoi(value);
        else if (strcmp(key, "timeout_seconds") == 0)
            config->timeout_seconds = (uint16_t)atoi(value);
        else if (strcmp(key, "log_level") == 0)
        {
            strncpy(config->log_level, value, sizeof(config->log_level) - 1);
            config->log_level[sizeof(config->log_level) - 1] = '\0';
        }
        else if (strcmp(key, "log_file") == 0)
        {
            strncpy(config->log_file, value, sizeof(config->log_file) - 1);
            config->log_file[sizeof(config->log_file) - 1] = '\0';
        }
        else if (strcmp(key, "log_to_stderr") == 0)
        {
            config->log_to_stderr = (strncmp(value, "true", 4) == 0);
        }
        // Unknown keys are ignored
    }

    fclose(fp);
    return 0;
} // end load_server_config_from_file


/**
 * @brief Logs the parsed server configuration.
 * 
 * Displays relevant network and logging parameters for diagnostic purposes.
 *
 * @param config Pointer to the server configuration struct.
 */
void log_server_config(const server_config_t *config)
{
    if (!config)
    {
        LOG_WARNING("log_server_config: NULL config pointer.");
        return;
    }

    LOG_INFO("========= Server Configuration =========");
    LOG_INFO("  Config Source:     %s", 
             config->source_path[0] ? config->source_path : "(none)");

    LOG_INFO("  Network Settings:");
    LOG_INFO("    Port:             %u", config->port);
    LOG_INFO("    Max Clients:      %u", config->max_clients);
    LOG_INFO("    Max Backlog:      %u", config->max_backlog);
    LOG_INFO("    Timeout (sec):    %u", config->timeout_seconds);
    LOG_INFO("    IP Version:       %s", config->enable_ipv6 ? "IPv6" : "IPv4");
    LOG_INFO("    Transport:        %s", config->enable_udp ? "UDP" : "TCP");

    LOG_INFO("  Logging Settings:");
    LOG_INFO("    Log Level:        %s", config->log_level);
    LOG_INFO("    Log File:         %s", config->log_file[0] ? config->log_file : "(none)");
    LOG_INFO("    Log to stderr:    %s", config->log_to_stderr ? "true" : "false");
    LOG_INFO("=========================================");
} // end log_server_config


/**
 * @brief Parses command-line arguments and populates config.
 * 
 * This function updates the server_config_t fields based on
 * command-line flags. 
 * 
 * @note This is called BEFORE logging is initialized.
 *
 * Supports:
 * - `-c path/to/server.conf`  → Load config from file
 * - `-p 1234`                 → Override port
 * - `-h`                      → Show help and exit
 *
 * Applies the following precedence:
 *   1. Default values
 *   2. Config file values (if provided with -c)
 *   3. CLI overrides (e.g., -p)
 *
 * @param argc Argument count from main().
 * @param argv Argument vector from main().
 * @param config Pointer to server_config_t to populate.
 * @return 0 on success, -1 on failure.
 */
int parse_arguments_server(int argc, char *argv[], server_config_t *config) 
{
    if (!config)
    {
        fprintf(stderr, "[!] parse_arguments_server: NULL config pointer.\n");
        return -1;
    }

    // Step 1: Initialize default config values
    if (!init_default_config(config))
    {
        fprintf(stderr, "[!] Failed to initialize default config.\n");
        return -1;
    }

    int opt, option_index = 0;

    // Temporary CLI values
    char config_filename[MAX_FILEPATH] = {0};
    int cli_port = -1;

    // First pass: parse and store CLI input
    optind = 1;
    while ((opt = getopt_long(argc, argv, "c:p:h", long_options_server, &option_index)) != -1)
    {
        switch (opt)
        {
            case 'c':
                strncpy(config_filename, optarg, MAX_FILEPATH - 1);
                break;
            case 'p':
                cli_port = atoi(optarg);
                break;
            case 'h':
                print_usage_server(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                fprintf(stderr, "Unknown option: -%c\n", opt);
                print_usage_server(argv[0]);
                return -1;
        }
    }

    // Step 2: If no config path provided, use fallback default
    if (config_filename[0] == '\0')
    {
        strncpy(config_filename, SERVER_DEFAULT_CONFIG_FILE, MAX_FILEPATH - 1);
    }

    // Step 3: Try loading config file
    if (load_server_config_from_file(config, config_filename) != 0)
    {
        fprintf(stderr, "[!] Warning: Failed to load config file: %s\n", config_filename);
        fprintf(stderr, "[!] Using hardcoded defaults and CLI overrides only.\n");
    }
    else
    {
        strncpy(config->source_path, config_filename, MAX_FILEPATH - 1);
        fprintf(stderr, "[*] Loaded config file: %s\n", config_filename);
    }

    // Step 4: Apply any CLI overrides
    if (cli_port > 0 && cli_port <= MAX_PORT)
    {
        config->port = cli_port;
    }

    return 0;
} // end parse_arguments_server


/**
 * @brief Displays the usage information for the server application.
 *
 * @param name Name of the executable, `argv[0]` from command-line arguments.
 */
void print_usage_server(char name[])
{
    printf(
    "\nUsage: %s [options]\n"
    "Options:\n"
    "    -c, --config [FILE]        Path to server configuration file\n"
    "                               (default: config/server.conf)\n"
    "    -p, --port [PORT]          Port to listen on\n"
    "                               (default: %d, range: %d-%d)\n"
    "    -h, --help                 Display this help message\n",
        name, SERVER_DEFAULT_PORT, MIN_PORT, MAX_PORT);
} // end print_usage_server


/**
 * @brief Resolves the absolute path to a file in the project's config directory.
 *
 * @param[in] rel_path      Filename in the config/ directory (e.g., "server.conf").
 * @param[out] out_path     Buffer to store the full resolved path.
 * @param[in] max_len       Size of the output buffer.
 * @return 0 on success, -1 on failure.
 */
int resolve_config_path(const char *rel_path, char *out_path, size_t max_len)
{
    if (rel_path == NULL || out_path == NULL || max_len == 0)
    {
        LOG_ERROR("resolve_config_path: Invalid arguments.");
        return -1;
    }

    char exe_path[MAX_FILEPATH];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1)
    {
        LOG_ERROR(
            "Failed to read /proc/self/exe: Cannot resolve executable path.");
        return -1;
    }
    exe_path[len] = '\0';

    // dirname() modifies its input, so copy first
    char exe_path_copy[MAX_FILEPATH];
    strncpy(exe_path_copy, exe_path, sizeof(exe_path_copy) - 1);
    exe_path_copy[sizeof(exe_path_copy) - 1] = '\0';

    char *exe_dir = dirname(exe_path_copy);
    if (exe_dir == NULL)
    {
        LOG_ERROR("Failed to resolve directory name from executable path.");
        return -1;
    }

    int written = snprintf(
            out_path, max_len, "%s/../%s%s", exe_dir, CONFIG_DIR, rel_path);
    if (written < 0 || (size_t)written >= max_len)
    {
        LOG_ERROR("Path too long when resolving config path to '%s'.", 
                rel_path);
        return -1;
    }

    LOG_DEBUG("Resolved path to '%s': %s", rel_path, out_path);
    return 0;
} // end resolve_config_path


int resolve_config_subpath(
    const char *input_filename, 
    char *resolved_path, 
    size_t max_len)
{
    if (!input_filename || !resolved_path || max_len == 0)
    {
        LOG_ERROR("resolve_config_subpath: Invalid arguments.");
        return -1;
    }

    // Construct full path under the data/ folder
    char temp_path[MAX_FILEPATH] = {0};
    int written = snprintf(
            temp_path, sizeof(temp_path), "%s%s", CONFIG_DIR, input_filename);

    if (written <= 0 || (size_t)written >= sizeof(temp_path))
    {
        LOG_ERROR("resolve_config_subpath: Constructed path too long.");
        return -1;
    }

    // Canonicalize the path
    char real_path_buf[MAX_FILEPATH] = {0};
    if (!realpath(temp_path, real_path_buf))
    {
        LOG_ERROR("resolve_config_subpath: Failed to canonicalize '%s'.", 
            temp_path);
        return -1;
    }

    // Ensure path is still within the config folder
    if (strstr(real_path_buf, "/config/") == NULL)
    {
        LOG_ERROR("resolve_config_subpath: Unsafe path detected ('%s').", 
            real_path_buf);
        return -1;
    }

    // Copy final validated path
    strncpy(resolved_path, real_path_buf, max_len - 1);
    resolved_path[max_len - 1] = '\0';

    LOG_DEBUG("Validated subpath: %s", resolved_path);
    return 0;
} // end resolve_config_subpath


/**
 * @brief Resolves the absolute path to a file in the project's data directory.
 *
 * @param[in] rel_path      Filename in the data directory (e.g., "users.db").
 * @param[out] out_path     Buffer to store the full resolved path.
 * @param[in] max_len       Size of the output buffer.
 * @return 0 on success, -1 on failure.
 */
int resolve_data_path(const char *rel_path, char *out_path, size_t max_len)
{
    if (rel_path == NULL || out_path == NULL || max_len == 0)
    {
        LOG_ERROR("resolve_data_path: Invalid arguments.");
        return -1;
    }

    char exe_path[MAX_FILEPATH];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1)
    {
        LOG_ERROR(
            "Failed to read /proc/self/exe: Cannot resolve executable path.");
        return -1;
    }
    exe_path[len] = '\0';

    char *exe_dir = dirname(exe_path);
    if (exe_dir == NULL)
    {
        LOG_ERROR("Failed to resolve directory name from executable path.");
        return -1;
    }

    int written = snprintf(
            out_path, max_len, "%s/../data/%s", exe_dir, rel_path);
    if (written < 0 || (size_t)written >= max_len)
    {
        LOG_ERROR("Path too long when resolving data path to '%s'.", 
                rel_path);
        return -1;
    }

    LOG_DEBUG("Resolved path to '%s': %s", rel_path, out_path);
    return 0;
} // end resolve_data_path


/**
 * @brief Initializes and returns a TCP server socket.
 *
 * @param config Pointer to a valid server configuration struct.
 * @return Valid socket file descriptor on success, or -1 on error.
 */
int setup_server_socket(const server_config_t *config)
{
    if (config == NULL) 
    {
        LOG_FATAL("[!] setup_server_socket called with NULL config.");
        return -1;
    }

    // Convert the integer port number to a string representation.
    char port_str[6];  // Max length port number, 5 digits + null terminator
    snprintf(port_str, sizeof(port_str), "%d", config->port);

    // Initialize the server socket with the string representation of the port.
    int server_socket = initialize_server_socket(
        port_str, config->enable_ipv6, config->enable_udp, BACKLOG);
    if (server_socket == -1)
    {
        LOG_ERROR(
            "[!] Failed to initialize server socket: %s", strerror(errno));
    }

    LOG_DEBUG("[+] Server socket initialized on port %s", port_str);
    return server_socket;
} // end setup_server_socket


/**
 * @brief Validates and converts a string to an integer.
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
    const char *flag_name)
{
    char *endptr;
    errno = 0; // Reset errno before parsing

    // Convert the string to a long integer
    long result = strtol(value, &endptr, 10);

    // Check for conversion errors
    if (errno == ERANGE || 
        result > INT_MAX || 
        result < INT_MIN || 
        *endptr != '\0')
    {
        LOG_ERROR("Invalid value '%s' for %s. Using default: %d.", 
                value, flag_name, default_value);
        return default_value;
    }

    // Ensure the value is within the specified range
    if (result < min || result > max) 
    {
        fprintf(stderr, "Error: Value '%ld' for %s is out of range (%d-%d). "
                        "Using default value: %d.\n",
                        result, flag_name, min, max, default_value);
        return default_value;
    }

    return (int)result;
} // end validate_int
