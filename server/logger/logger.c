#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>

static FILE *log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Initialize logger
 * @param filename Log file path (NULL for stdout only)
 * @return 0 on success, -1 on error
 */
int logger_init(const char *filename) {
    pthread_mutex_lock(&log_mutex);

    if (filename) {
        log_file = fopen(filename, "a");
        if (!log_file) {
            pthread_mutex_unlock(&log_mutex);
            return -1;
        }
    }
    pthread_mutex_unlock(&log_mutex);
    return 0;
}

/**
 * @brief Close Logger
 */
void logger_close() {
    pthread_mutex_lock(&log_mutex);
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
    pthread_mutex_unlock(&log_mutex);
}

/**
 * @brief Convert log level to string
 * @param level Log level
 * @return String representation of log level
 */
const char* log_level_string(LogLevel level) {
    switch(level) {
        case LOG_INFO: return "INFO";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Log event
 */
void log_event(LogLevel level, const char *username, const char *action, const char *format, ...) {
    pthread_mutex_lock(&log_mutex);

    // Get timestamp
    time_t now = time(NULL);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // Format message
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // Format log entry
    char log_entry[2048];
    snprintf(log_entry, sizeof(log_entry), "[%s] [%s] [%s] %s: %s\n",
             time_str,
             log_level_string(level),
             username ? username : "SYSTEM",
             action,
             message);

    // Write to log file 
    if (log_file) {
        fputs(log_entry, log_file);
        fflush(log_file);
    }

    // Also write to stdout for important messages
    if (level == LOG_ERROR || level == LOG_WARNING) {
        fputs(log_entry, stdout);
        fflush(stdout);
    }

    pthread_mutex_unlock(&log_mutex);
}