
#include "practice.h"
#include "../server.h"
#include "../auth/auth.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/**
 * @brief Handle PRACTICE command
 */
void handle_practice(Server *server, ClientSession *client, Message *msg)
{
    // Check authentication
    if (!check_authentication(client))
    {
        send_error(client->socket_fd, CODE_NOT_LOGGED, "Not authenticated");
        return;
    }

    // Validate params
    if (msg->param_count < 2)
    {
        send_error(client->socket_fd, CODE_SYNTAX_ERROR,
                   "Usage: PRACTICE num_questions|time_limit");
        return;
    }

    int num_questions = atoi(msg->params[0]);
    int time_limit = atoi(msg->params[1]);

    // Validate number of questions
    if (num_questions < 5 || num_questions > 50)
    {
        send_error(client->socket_fd, CODE_INVALID_PARAMS,
                   "Number of questions must be 5-50");
        db_log_activity(&server->db, "WARNING", client->username,
                        "PRACTICE", "Invalid num_questions");
        return;
    }

    // Validate time limit
    if (time_limit < 5 || time_limit > 120)
    {
        send_error(client->socket_fd, CODE_INVALID_PARAMS,
                   "Time limit must be 5-120 minutes");
        db_log_activity(&server->db, "WARNING", client->username,
                        "PRACTICE", "Invalid time_limit");
        return;
    }

    // Generate practice session ID
    char practice_id[32];
    snprintf(practice_id, sizeof(practice_id), "practice_%ld", time(NULL));

    // Create practice session in database
    if (db_create_practice_session(&server->db, practice_id, client->username,
                                   num_questions, time_limit) < 0)
    {
        send_error(client->socket_fd, CODE_INTERNAL_ERROR,
                   "Failed to create practice session");
        db_log_activity(&server->db, "ERROR", client->username,
                        "PRACTICE", "Database error");
        return;
    }

    // Get random questions from database (returns JSON with questions + practice_id)
    char *questions_json = db_get_random_questions(&server->db, num_questions);
    if (!questions_json)
    {
        send_error(client->socket_fd, CODE_INTERNAL_ERROR,
                   "Failed to get questions");
        db_log_activity(&server->db, "ERROR", client->username,
                        "PRACTICE", "Failed to get questions");
        return;
    }

    // Build final JSON with practice_id and time_limit
    char final_json[MAX_DATA_SIZE];
    snprintf(final_json, sizeof(final_json),
             "{\"practice_id\":\"%s\",\"time_limit\":%d,\"questions\":%s}",
             practice_id, time_limit, questions_json);

    free(questions_json);

    // Send data with length prefixing
    char buffer[MAX_DATA_SIZE];
    int len = create_data_message(CODE_DATA, final_json, strlen(final_json),
                                  buffer, sizeof(buffer));

    if (len > 0)
    {
        send_full(client->socket_fd, buffer, len);

        // Update client state
        client->state = STATE_IN_PRACTICE;

        // Log activity
        char details[256];
        snprintf(details, sizeof(details),
                 "Practice session '%s': %d questions, %d minutes",
                 practice_id, num_questions, time_limit);
        db_log_activity(&server->db, "INFO", client->username,
                        "PRACTICE", details);

        printf("[PRACTICE] User '%s' started practice session '%s'\n",
               client->username, practice_id);
    }
    else
    {
        send_error(client->socket_fd, CODE_INTERNAL_ERROR,
                   "Response too large");
    }
}

/**
 * @brief Handle SUBMIT_PRACTICE command
 */
void handle_submit_practice(Server *server, ClientSession *client, Message *msg)
{
    // Check authentication
    if (!check_authentication(client))
    {
        send_error(client->socket_fd, CODE_NOT_LOGGED, "Not authenticated");
        return;
    }

    // Validate params
    if (msg->param_count < 2)
    {
        send_error(client->socket_fd, CODE_SYNTAX_ERROR,
                   "Usage: SUBMIT_PRACTICE practice_id|answers");
        return;
    }

    const char *practice_id = msg->params[0];
    const char *answers = msg->params[1];

    // Get correct answers for this practice session
    int score = 0;
    int total = 0;
    char correct_answers[256];

    if (db_get_practice_answers(&server->db, practice_id,
                                correct_answers, &total) < 0)
    {
        send_error(client->socket_fd, CODE_INVALID_PARAMS,
                   "Invalid practice session");
        db_log_activity(&server->db, "WARNING", client->username,
                        "SUBMIT_PRACTICE", "Invalid practice_id");
        return;
    }

    // Count correct answers
    char *answer_copy = strdup(answers);
    char *answer_tok = strtok(answer_copy, ",");
    int idx = 0;

    while (answer_tok && idx < total)
    {
        if (answer_tok[0] == correct_answers[idx])
        {
            score++;
        }
        answer_tok = strtok(NULL, ",");
        idx++;
    }

    free(answer_copy);

    // Save score
    db_submit_practice(&server->db, practice_id, score);

    // Send result
    char response[128];
    snprintf(response, sizeof(response), "PRACTICE_RESULT %d|%d", score, total);
    send_error(client->socket_fd, CODE_PRACTICE_RESULT, response);

    // Update state back to authenticated
    client->state = STATE_AUTHENTICATED;

    // Log activity
    char details[256];
    snprintf(details, sizeof(details), "Score: %d/%d", score, total);
    db_log_activity(&server->db, "INFO", client->username,
                    "SUBMIT_PRACTICE", details);

    printf("[SUBMIT_PRACTICE] User '%s' scored %d/%d in '%s'\n",
           client->username, score, total, practice_id);
}

// ==========================================
// FILE: server/logger/logger.h
// ==========================================
#ifndef LOGGER_H
#define LOGGER_H

#include <time.h>

/**
 * @brief Log level
 */
typedef enum
{
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} LogLevel;

/**
 * @brief Initialize logger
 * @param filename Log file path (NULL for stdout only)
 * @return 0 on success, -1 on error
 */
int logger_init(const char *filename);

/**
 * @brief Close logger
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
void log_event(LogLevel level, const char *username, const char *action,
               const char *format, ...);

/**
 * @brief Get level string
 */
const char *log_level_string(LogLevel level);

#endif // LOGGER_H

// ==========================================
// FILE: server/logger/logger.c
// ==========================================
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
 */
int logger_init(const char *filename)
{
    pthread_mutex_lock(&log_mutex);

    if (filename)
    {
        log_file = fopen(filename, "a");
        if (!log_file)
        {
            pthread_mutex_unlock(&log_mutex);
            return -1;
        }
    }

    pthread_mutex_unlock(&log_mutex);
    return 0;
}

/**
 * @brief Close logger
 */
void logger_close()
{
    pthread_mutex_lock(&log_mutex);

    if (log_file)
    {
        fclose(log_file);
        log_file = NULL;
    }

    pthread_mutex_unlock(&log_mutex);
}

/**
 * @brief Get log level string
 */
const char *log_level_string(LogLevel level)
{
    switch (level)
    {
    case LOG_INFO:
        return "INFO";
    case LOG_WARNING:
        return "WARNING";
    case LOG_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief Log event
 */
void log_event(LogLevel level, const char *username, const char *action,
               const char *format, ...)
{
    pthread_mutex_lock(&log_mutex);

    // Get timestamp
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S",
             localtime(&now));

    // Format message
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // Format log entry
    char log_entry[2048];
    snprintf(log_entry, sizeof(log_entry), "[%s] [%s] [%s] %s: %s\n",
             timestamp,
             log_level_string(level),
             username ? username : "SYSTEM",
             action,
             message);

    // Write to file
    if (log_file)
    {
        fputs(log_entry, log_file);
        fflush(log_file);
    }

    // Also write to stdout for important messages
    if (level == LOG_WARNING || level == LOG_ERROR)
    {
        fputs(log_entry, stdout);
    }

    pthread_mutex_unlock(&log_mutex);
}
