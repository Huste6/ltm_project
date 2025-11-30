#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/**
 * @brief Log levels
 */
typedef enum {
    LOG_INFO,    // dùng cho các thông tin chung
    LOG_WARNING, // dùng cho các cảnh báo
    LOG_ERROR    // dùng cho các lỗi
} LogLevel;

/**
 * @brief Initialize logger
 * @param filename Log file path (NULL for stdout only)
 * @return 0 on success, -1 on error
 */
int logger_init(const char *filename);

/**
 * @brief Close Logger
 */    
void logger_close();

/**
 * @brief Log a message
 * @param level Log level
 * @param username Username (can be NULL for system messages)
 * @param action Action/Event name
 * @param format Printf-style format string
 * @param ... Variable arguments
 * 
 * Format: [TIMESTAMP] [LEVEL] [USERNAME] action: message
 * Example: [2025-11-30 14:30:00] [INFO] [john123] LOGIN: Successful login
 */
void log_event(LogLevel level, const char *username, const char *action, const char *format, ...);

/**
 * @brief Convert log level to string
 * @param level Log level
 * @return String representation of log level
 */
const char* log_level_to_string(LogLevel level);

#endif // LOGGER_H