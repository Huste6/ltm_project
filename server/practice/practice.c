
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
        send_error_or_response(client->socket_fd, CODE_NOT_LOGGED, "Not authenticated");
        return;
    }

    // Validate params
    if (msg->param_count < 2)
    {
        send_error_or_response(client->socket_fd, CODE_SYNTAX_ERROR,
                               "Usage: PRACTICE num_questions|time_limit");
        return;
    }

    int num_questions = atoi(msg->params[0]);
    int time_limit = atoi(msg->params[1]);

    // Validate number of questions
    if (num_questions < 5 || num_questions > 50)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS,
                               "Number of questions must be 5-50");
        db_log_activity(server->db, "WARNING", client->username,
                        "PRACTICE", "Invalid num_questions");
        return;
    }

    // Validate time limit
    if (time_limit < 1 || time_limit > 120)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS,
                               "Time limit must be 1-120 minutes");
        db_log_activity(server->db, "WARNING", client->username,
                        "PRACTICE", "Invalid time_limit");
        return;
    }

    // Generate practice session ID
    char practice_id[32];
    snprintf(practice_id, sizeof(practice_id), "practice_%ld", time(NULL));

    // Create practice session in database
    if (db_create_practice_session(server->db, practice_id, client->username,
                                   num_questions, time_limit) < 0)
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR,
                               "Failed to create practice session");
        db_log_activity(server->db, "ERROR", client->username,
                        "PRACTICE", "Database error");
        return;
    }

    // Get random questions from database (returns JSON with questions + practice_id)
    char *questions_json = db_get_random_questions(server->db, num_questions);
    if (!questions_json)
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR,
                               "Failed to get questions");
        db_log_activity(server->db, "ERROR", client->username,
                        "PRACTICE", "Failed to get questions");
        return;
    }

    // Extract question IDs from JSON and save to practice_questions table
    char question_ids[512] = "";
    const char *search = questions_json;
    int q_count = 0;

    while ((search = strstr(search, "\"question_id\":")) != NULL && q_count < num_questions)
    {
        search += 14; // Move past "question_id":
        int q_id = atoi(search);

        if (q_count > 0)
            strcat(question_ids, ",");

        char id_str[16];
        snprintf(id_str, sizeof(id_str), "%d", q_id);
        strcat(question_ids, id_str);

        q_count++;
        search++;
    }

    // Save question IDs to practice_questions table
    if (db_save_practice_questions(server->db, practice_id, question_ids, num_questions) < 0)
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR,
                               "Failed to save practice questions");
        free(questions_json);
        return;
    }

    // Build final JSON with practice_id and time_limit
    char final_json[MAX_DATA_SIZE];
    snprintf(final_json, sizeof(final_json),
             "{\"practice_id\":\"%s\",\"time_limit\":%d,\"num_questions\":%d,\"questions\":%s}",
             practice_id, time_limit, num_questions, questions_json);

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
        db_log_activity(server->db, "INFO", client->username,
                        "PRACTICE", details);

        printf("[PRACTICE] User '%s' started practice session '%s'\n",
               client->username, practice_id);
    }
    else
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR,
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
        send_error_or_response(client->socket_fd, CODE_NOT_LOGGED, "Not authenticated");
        return;
    }

    // Validate params
    if (msg->param_count < 2)
    {
        send_error_or_response(client->socket_fd, CODE_SYNTAX_ERROR,
                               "Usage: SUBMIT_PRACTICE practice_id|answers");
        return;
    }

    const char *practice_id = msg->params[0];
    const char *answers = msg->params[1];

    // Check if already submitted
    if (db_check_practice_submitted(server->db, practice_id))
    {
        send_error_or_response(client->socket_fd, CODE_ALREADY_SUBMITTED,
                               "Practice already submitted");
        return;
    }

    // Get practice info to check timeout
    char session_username[MAX_USERNAME_LEN];
    time_t start_time;
    int time_limit;

    if (db_get_practice_info(server->db, practice_id, session_username, &start_time, &time_limit) < 0)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS,
                               "Invalid practice session");
        db_log_activity(server->db, "WARNING", client->username,
                        "SUBMIT_PRACTICE", "Invalid practice_id");
        return;
    }

    // Check if practice belongs to this user
    if (strcmp(session_username, client->username) != 0)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS,
                               "Practice session does not belong to you");
        return;
    }

    // Check timeout
    time_t now = time(NULL);
    time_t elapsed = now - start_time;
    time_t time_limit_seconds = time_limit * 60;

    int is_timeout = 0;
    if (elapsed > time_limit_seconds)
    {
        is_timeout = 1;
        db_log_activity(server->db, "WARNING", client->username,
                        "SUBMIT_PRACTICE", "Time expired during submission");
    }

    // Get correct answers for this practice session
    int score = 0;
    int total = 0;
    char correct_answers[256];

    if (db_get_practice_answers(server->db, practice_id,
                                correct_answers, &total) < 0)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS,
                               "Failed to get correct answers");
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
    db_submit_practice(server->db, practice_id, score);

    // Send result - include timeout flag in message if applicable
    char response[256];
    if (is_timeout)
    {
        snprintf(response, sizeof(response), "%d|%d|TIMEOUT", score, total);
    }
    else
    {
        snprintf(response, sizeof(response), "%d|%d", score, total);
    }
    send_error_or_response(client->socket_fd, CODE_PRACTICE_RESULT, response);

    // Update state back to authenticated
    client->state = STATE_AUTHENTICATED;

    // Log activity
    char details[256];
    if (is_timeout)
    {
        snprintf(details, sizeof(details), "Score: %d/%d (TIMEOUT)", score, total);
    }
    else
    {
        snprintf(details, sizeof(details), "Score: %d/%d", score, total);
    }
    db_log_activity(server->db, "INFO", client->username,
                    "SUBMIT_PRACTICE", details);

    printf("[SUBMIT_PRACTICE] User '%s' scored %d/%d in '%s'%s\n",
           client->username, score, total, practice_id, is_timeout ? " (TIMEOUT)" : "");
}
