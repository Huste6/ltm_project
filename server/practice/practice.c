
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
        send_error_or_response(client->socket_fd, CODE_SYNTAX_ERROR, "Usage: PRACTICE num_questions|time_limit");
        return;
    }

    int num_questions = atoi(msg->params[0]);
    int time_limit = atoi(msg->params[1]);

    // Validate number of questions
    if (num_questions < 5 || num_questions > 50)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS, "Number of questions must be 5-50");
        db_log_activity(server->db, "WARNING", client->username, "PRACTICE", "Invalid num_questions");
        return;
    }

    // Validate time limit
    if (time_limit < 1 || time_limit > 120)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS, "Time limit must be 1-120 minutes");
        db_log_activity(server->db, "WARNING", client->username, "PRACTICE", "Invalid time_limit");
        return;
    }

    // Generate practice session ID
    char practice_id[32];
    snprintf(practice_id, sizeof(practice_id), "practice_%ld", time(NULL));

    // Create practice session in database
    if (db_create_practice_session(server->db, practice_id, client->username, num_questions, time_limit) < 0)
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Failed to create practice session");
        db_log_activity(server->db, "ERROR", client->username, "PRACTICE", "Database error");
        return;
    }

    // Get random questions from database (returns JSON with questions + practice_id)
    char *questions_json = db_get_random_questions(server->db, num_questions);
    if (!questions_json)
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Failed to get questions");
        db_log_activity(server->db, "ERROR", client->username, "PRACTICE", "Failed to get questions");
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
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Failed to save practice questions");
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
    int len = create_data_message(CODE_DATA, final_json, strlen(final_json), buffer, sizeof(buffer));

    if (len > 0)
    {
        send_full(client->socket_fd, buffer, len);

        // Update client state and init practice buffer
        client->state = STATE_IN_PRACTICE;
        strncpy(client->practice_id, practice_id, sizeof(client->practice_id) - 1);
        client->practice_id[sizeof(client->practice_id) - 1] = '\0';
        client->practice_total_questions = num_questions;
        memset(client->practice_answers, 0, sizeof(client->practice_answers));

        // Log activity
        char details[256];
        snprintf(details, sizeof(details), "Practice session '%s': %d questions, %d minutes", practice_id, num_questions, time_limit);
        db_log_activity(server->db, "INFO", client->username, "PRACTICE", details);

        printf("[PRACTICE] User '%s' started practice session '%s'\n", client->username, practice_id);
    }
    else
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Response too large");
    }
}

/**
 * @brief Handle SAVE_PRACTICE_ANSWER command - Save answer to buffer
 */
void handle_save_practice_answer(Server *server, ClientSession *client, Message *msg)
{
    // Check authentication
    if (!check_authentication(client))
    {
        send_error_or_response(client->socket_fd, CODE_NOT_LOGGED, "Not authenticated");
        return;
    }

    // Validate params: SAVE_PRACTICE_ANSWER practice_id|question_index|answer
    if (msg->param_count < 3)
    {
        send_error_or_response(client->socket_fd, CODE_SYNTAX_ERROR, "Usage: SAVE_PRACTICE_ANSWER practice_id|question_index|answer");
        return;
    }

    const char *practice_id = msg->params[0];
    int question_index = atoi(msg->params[1]); // 0-based index
    char answer = msg->params[2][0];           // A/B/C/D

    // Check if practice is already submitted (cleared by cleanup thread)
    if (strlen(client->practice_id) == 0)
    {
        // Practice already submitted/timed out by cleanup thread
        // Silently ignore this request (don't send error back)
        printf("[SAVE_PRACTICE_ANSWER] Ignoring late save for already-submitted practice\n");
        return;
    }

    // Validate practice_id matches client's current session
    if (strcmp(client->practice_id, practice_id) != 0)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS, "Invalid practice session");
        return;
    }

    // Check timeout
    char session_username[MAX_USERNAME_LEN];
    time_t start_time;
    int time_limit;

    if (db_get_practice_info(server->db, practice_id, session_username, &start_time, &time_limit) < 0)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS, "Practice session not found");
        return;
    }

    time_t now = time(NULL);
    time_t elapsed = now - start_time;
    time_t time_limit_seconds = time_limit * 60;

    if (elapsed > time_limit_seconds)
    {
        send_error_or_response(client->socket_fd, CODE_TIME_EXPIRED, "Practice time expired");
        db_log_activity(server->db, "WARNING", client->username, "SAVE_PRACTICE_ANSWER", "Time expired");
        return;
    }

    // Validate question_index
    if (question_index < 0 || question_index >= client->practice_total_questions)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS, "Invalid question index");
        return;
    }

    // Validate answer
    if (answer < 'A' || answer > 'D')
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS, "Answer must be A, B, C, or D");
        return;
    }

    // Save answer to buffer
    client->practice_answers[question_index] = answer;

    // Send success response
    send_error_or_response(client->socket_fd, CODE_ANSWER_SAVED, "Answer saved");

    printf("[SAVE_PRACTICE_ANSWER] User '%s' saved answer %c for question %d in '%s'\n", client->username, answer, question_index + 1, practice_id);
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

    // Validate params: now only practice_id (answers in buffer client->practice_answers)
    if (msg->param_count < 1)
    {
        send_error_or_response(client->socket_fd, CODE_SYNTAX_ERROR, "Usage: SUBMIT_PRACTICE practice_id");
        return;
    }

    const char *practice_id = msg->params[0];

    // Check if already submitted
    if (db_check_practice_submitted(server->db, practice_id))
    {
        send_error_or_response(client->socket_fd, CODE_ALREADY_SUBMITTED, "Practice already submitted");
        return;
    }

    // Get practice info to check timeout
    char session_username[MAX_USERNAME_LEN];
    time_t start_time;
    int time_limit;

    if (db_get_practice_info(server->db, practice_id, session_username, &start_time, &time_limit) < 0)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS, "Invalid practice session");
        db_log_activity(server->db, "WARNING", client->username, "SUBMIT_PRACTICE", "Invalid practice_id");
        return;
    }

    // Check if practice belongs to this user
    if (strcmp(session_username, client->username) != 0)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS, "Practice session does not belong to you");
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
        db_log_activity(server->db, "WARNING", client->username, "SUBMIT_PRACTICE", "Time expired during submission");
    }

    // Get correct answers for this practice session
    int score = 0;
    int total = 0;
    char correct_answers[256];

    if (db_get_practice_answers(server->db, practice_id, correct_answers, &total) < 0)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS, "Failed to get correct answers");
        return;
    }

    // Grade from client->practice_answers buffer
    for (int i = 0; i < total; i++)
    {
        char user_answer = client->practice_answers[i];

        // If unanswered (0), treat as wrong
        if (user_answer != 0 && user_answer == correct_answers[i])
        {
            score++;
        }
    }

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

    // Update state back to authenticated and clear practice buffer
    client->state = STATE_AUTHENTICATED;
    memset(client->practice_id, 0, sizeof(client->practice_id));
    memset(client->practice_answers, 0, sizeof(client->practice_answers));

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
    db_log_activity(server->db, "INFO", client->username, "SUBMIT_PRACTICE", details);

    printf("[SUBMIT_PRACTICE] User '%s' scored %d/%d in '%s'%s\n", client->username, score, total, practice_id, is_timeout ? " (TIMEOUT)" : "");
}

/**
 * @brief Force submit practice on timeout - called by cleanup thread
 */
int force_submit_practice(Server *server, ClientSession *client, const char *practice_id)
{
    // Skip if already submitted
    if (db_check_practice_submitted(server->db, practice_id))
    {
        return 0; // Already submitted
    }

    // Get correct answers and total questions
    int score = 0;
    int total = 0;
    char correct_answers[256];

    if (db_get_practice_answers(server->db, practice_id, correct_answers, &total) < 0)
    {
        printf("[FORCE_SUBMIT_PRACTICE] Failed to get correct answers for '%s'\n", practice_id);
        return -1;
    }

    // Grade from client->practice_answers buffer
    for (int i = 0; i < total; i++)
    {
        char user_answer = client->practice_answers[i];

        // If unanswered (0), treat as wrong
        if (user_answer != 0 && user_answer == correct_answers[i])
        {
            score++;
        }
    }

    // Save score
    if (db_submit_practice(server->db, practice_id, score) < 0)
    {
        printf("[FORCE_SUBMIT_PRACTICE] Failed to save result for user '%s' in '%s'\n", client->username, practice_id);
        return -1;
    }

    // Update client state
    client->state = STATE_AUTHENTICATED;
    memset(client->practice_id, 0, sizeof(client->practice_id));
    memset(client->practice_answers, 0, sizeof(client->practice_answers));

    // Log activity
    char details[256];
    snprintf(details, sizeof(details), "AUTO_SUBMIT (timeout): Score %d/%d", score, total);
    db_log_activity(server->db, "INFO", client->username, "FORCE_SUBMIT_PRACTICE", details);

    printf("[FORCE_SUBMIT_PRACTICE] User '%s' auto-submitted: %d/%d in practice '%s'\n",
           client->username, score, total, practice_id);

    return 0;
}
