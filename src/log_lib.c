/**
 * @file log_lib.c
 * @brief Implementation of the logging library.
 *
 * Provides runtime-configurable logging for different severity levels.
 * Supports formatted messages and allows filtering logs based on severity.
 */

#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <stdbool.h>

#define LOG_TIMESTAMP_LEN 20
#define LOG_LINE_MAX      512

#include "../include/log_lib.h"

/* ===================== Internal State ===================== */

/** @brief Default global log level (modifiable at runtime). */
log_level_t global_log_level = LOG_LEVEL_INFO;
static FILE *log_output = NULL;
static bool also_log_to_stderr = false;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;


/* ===================== Helper Functions ===================== */
/**
 * @brief Get symbolic prefix string for a log level.
 */
static const char *level_prefix(log_level_t level)
{
    switch (level)
    {
        case LOG_LEVEL_DEBUG:   return "[~]";
        case LOG_LEVEL_INFO:    return "[*]";
        case LOG_LEVEL_WARNING: return "[!]";
        case LOG_LEVEL_ERROR:   return "[!]";
        case LOG_LEVEL_FATAL:   return "[x]";
        default:                return "[?]";
    }
} // end level_prefix

/**
 * @brief Trim whitespace from both ends of a string.
 */
static void trim(char *str)
{
    if (!str) return;

    char *end;
    while (isspace((unsigned char)*str)) str++;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) *end-- = '\0';
} // end trim

/* ===================== Public API ===================== */

log_level_t log_get_level(void) 
{
    return global_log_level;
} // end log_get_level

void log_set_level(log_level_t level) 
{
    global_log_level = level;
} // end log_set_level

void log_init(log_level_t level, FILE *file)
{
    global_log_level = level;
    log_output = (file != NULL) ? file : stderr;
    // only duplicate if output is file
    also_log_to_stderr = (log_output != stderr);
} // end log_init

bool log_init_from_file(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) return false;

    char line[LOG_LINE_MAX];
    char log_file_path[128] = {0};
    log_level_t level = LOG_LEVEL_INFO;
    bool to_stderr = false;

    while (fgets(line, sizeof(line), fp))
    {
        if (line[0] == '#' || line[0] == '\n') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        trim(key);
        trim(val);

        if (strcmp(key, "log_level") == 0)
            level = parse_log_level(val);
        else if (strncmp(key, "log_file", 8) == 0)
            strncpy(log_file_path, val, sizeof(log_file_path) - 1);
        else if (strncmp(key, "log_to_stderr", 13) == 0)
            to_stderr = (strncmp(val, "true", 4) == 0);
    }

    fclose(fp);

    FILE *file_output = NULL;
    if (strlen(log_file_path) > 0)
    {
        file_output = fopen(log_file_path, "a");
        if (!file_output)
        {
            fprintf(stderr, "[log_lib]: Failed to open log file '%s'\n", 
                    log_file_path);
            return false;
        }
    }

    log_output = (file_output != NULL) ? file_output : stderr;
    global_log_level = level;
    also_log_to_stderr = to_stderr && (log_output != stderr);

    return true;
} // end log_init_from_file

void log_close(void)
{
    if (log_output && log_output != stderr)
        fclose(log_output);
} // end log_close

void log_message(
    log_level_t level, 
    const char* file, 
    int line, 
    const char* format, 
    ...) 
{
    if (level < global_log_level || global_log_level == LOG_LEVEL_NONE)
    {
        return;
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    char timestamp[LOG_TIMESTAMP_LEN];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    char line_buffer[LOG_LINE_MAX];

    pthread_mutex_lock(&log_mutex);

    snprintf(line_buffer, sizeof(line_buffer), "%s %s [%s:%d]: ",
             timestamp, level_prefix(level), file, line);

    fputs(line_buffer, log_output);

    va_list args;
    va_start(args, format);
    vfprintf(log_output, format, args);
    va_end(args);

    fputc('\n', log_output);
    fflush(log_output);

    if (also_log_to_stderr)
    {
        fputs(line_buffer, stderr);
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fputc('\n', stderr);
        fflush(stderr);
    }

    pthread_mutex_unlock(&log_mutex);
} // end log_message


void log_message_custom(
    const char *prefix,
    const char *file,
    int line,
    const char *format,
    ...)
{
    if (global_log_level == LOG_LEVEL_NONE)
    {
        return;
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    char timestamp[LOG_TIMESTAMP_LEN];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    char line_buffer[LOG_LINE_MAX];

    pthread_mutex_lock(&log_mutex);

    snprintf(line_buffer, sizeof(line_buffer), "%s %s [%s:%d]: ",
             timestamp, prefix, file, line);

    fputs(line_buffer, log_output);

    va_list args;
    va_start(args, format);
    vfprintf(log_output, format, args);
    va_end(args);

    fputc('\n', log_output);
    fflush(log_output);

    if (also_log_to_stderr)
    {
        fputs(line_buffer, stderr);
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fputc('\n', stderr);
        fflush(stderr);
    }

    pthread_mutex_unlock(&log_mutex);
} // end log_message_custom


/**
 * @brief Convert string to log_level_t (case-insensitive).
 */
log_level_t parse_log_level(const char *str)
{
    if (!str) return LOG_LEVEL_INFO;

    if (strncmp(str, "NONE", 4) == 0)    return LOG_LEVEL_NONE;
    if (strncmp(str, "DEBUG", 5) == 0)   return LOG_LEVEL_DEBUG;
    if (strncmp(str, "INFO", 4) == 0)    return LOG_LEVEL_INFO;
    if (strncmp(str, "WARNING", 7) == 0) return LOG_LEVEL_WARNING;
    if (strncmp(str, "ERROR", 5) == 0)   return LOG_LEVEL_ERROR;
    if (strncmp(str, "FATAL", 5) == 0)   return LOG_LEVEL_FATAL;

    return LOG_LEVEL_INFO; // default
} // end parse_log_level
