/**
 * @file log_lib.h
 * @brief Logging library for various log levels.
 *
 * Provides functions to log messages with different severity levels.
 * This module allows setting a global log level and supports formatted messages.
 * 
 * @note Additional macros are available for tagged events:
 * - LOG_SEND / LOG_RECV for socket activity
 * - LOG_CREATE / LOG_DESTROY for lifecycle tracking
 * - LOG_AUTH for authentication events
 */

#ifndef LOG_LIB_H
#define LOG_LIB_H

/* ========================= Macros and Constants ========================= */

/* Convenience macros with line breaks to fit within 80 characters */
#define LOG_DEBUG(...)   \
    log_message(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_INFO(...)    \
    log_message(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_WARNING(...) \
    log_message(LOG_LEVEL_WARNING, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_ERROR(...)   \
    log_message(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_FATAL(...)   \
    log_message(LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_SEND(...) \
    log_message_custom("[>]", __FILE__, __LINE__, __VA_ARGS__)

#define LOG_RECV(...) \
    log_message_custom("[<]", __FILE__, __LINE__, __VA_ARGS__)

#define LOG_CREATE(...) \
    log_message_custom("[+]", __FILE__, __LINE__, __VA_ARGS__)

#define LOG_DESTROY(...) \
    log_message_custom("[-]", __FILE__, __LINE__, __VA_ARGS__)

#define LOG_AUTH(...) \
    log_message_custom("[@]", __FILE__, __LINE__, __VA_ARGS__)

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

/**
 * @enum log_level_t
 * @brief Enumeration of logging levels.
 *
 * Defines various log severity levels used throughout the application.
 */
typedef enum {
    LOG_LEVEL_DEBUG = 0,    /**< Debugging messages */
    LOG_LEVEL_INFO,         /**< Informational messages */
    LOG_LEVEL_WARNING,      /**< Warnings */
    LOG_LEVEL_ERROR,        /**< Recoverable errors */
    LOG_LEVEL_FATAL,        /**< Critical errors */
    LOG_LEVEL_NONE          /**< Disable logging completely */
} log_level_t;


/**
 * @brief Global log level, modifiable at runtime.
 *
 * Determines the minimum severity level required for a log message
 * to be displayed. The default is LOG_LEVEL_INFO.
 */
extern log_level_t global_log_level;

/**
 * @brief Gets the current global log level.
 *
 * @return The current log level (e.g., LOG_LEVEL_DEBUG, LOG_LEVEL_INFO).
 */
log_level_t log_get_level(void);

/**
 * @brief Sets the global log level.
 *
 * @param level The log level to set (e.g., LOG_LEVEL_DEBUG, LOG_LEVEL_INFO).
 */
void log_set_level(log_level_t level);

/**
 * @brief Initializes the logging system with a specified log level and output.
 * 
 * Sets the global log level and designates the output stream for log messages.
 * If the file is NULL, logs are written to stderr.
 *
 * @param level Minimum severity log messages to display. (e.g., LOG_LEVEL_INFO)
 * @param file Output file stream (e.g., opened with fopen). (NULL for stderr)
 */
void log_init(log_level_t level, FILE *file);

/**
 * @brief Initializes logging based on a configuration file.
 * 
 * Parses a configuration file to set the log level, log file path, 
 * and whether to duplicate logs to stderr.
 *
 * @param path Path to config/logging.conf
 * @return true if successful, false otherwise
 */
bool log_init_from_file(const char *path);

/**
 * @brief Closes the current log output file, if one is open.
 *
 * If logging is directed to a file (not stderr), this function flushes
 * and closes the file stream. Calling this at shutdown to ensure all log 
 * data is written.
 */
void log_close(void);

/**
 * @brief Logs a formatted message with a specified severity level.
 *
 * Logs messages along with the source file and line number. Supports
 * formatted output similar to printf.
 *
 * @param level The log severity level.
 * @param file The source file where the log was triggered.
 * @param line The line number in the source file.
 * @param format The message format string (supports variadic arguments).
 */
void log_message(
    log_level_t level, 
    const char* file, 
    int line, 
    const char* format, 
    ...);

/**
 * @brief Logs a formatted message with a custom tag.
 *
 * @param prefix Custom prefix string (e.g., "[>]").
 * @param file The source file where the log was triggered.
 * @param line The line number in the source file.
 * @param format The message format string (supports variadic arguments).
 */
void log_message_custom(
    const char *prefix,
    const char *file,
    int line,
    const char *format,
    ...);

/**
 * @brief Convert string to log_level_t (case-insensitive).
 *
 * Accepts case-insensitive strings like "debug", "INFO", "Error", etc.
 * Returns LOG_LEVEL_INFO as a fallback for unknown strings.
 *
 * @param level_str The string representing a log level.
 * @return The corresponding log_level_t enum value.
 */
log_level_t parse_log_level(const char *str);

#endif // LOG_LIB_H