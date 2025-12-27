#include "exam.h"
#include "../server.h"
#include "../auth/auth.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/**
 * @brief Handle GET_EXAM command - return exam questions
 */
void handle_get_exam(Server *server, ClientSession *client, Message *msg)
{
    // Check authentication
    if (!check_authentication(client))
    {
        send_error_or_response(client->socket_fd, 221, "NOT_LOGGED");
        return;
    }

    // Validate params
    if (msg->param_count < 1)
    {
        send_error_or_response(client->socket_fd, 300, "BAD_COMMAND");
        return;
    }

    const char *room_id = msg->params[0];

    // Check room exists
    int status = db_get_room_status(server->db, room_id);
    if (status < 0)
    {
        send_error_or_response(client->socket_fd, 223, "ROOM_NOT_FOUND");
        db_log_activity(server->db, "WARNING", client->username, "GET_EXAM", "Room not found");
        return;
    }

    // Check room NOT_STARTED (status 0 = NOT_STARTED)
    if (status == 0)
    {
        send_error_or_response(client->socket_fd, 224, "ROOM_NOT_STARTED");
        db_log_activity(server->db, "WARNING", client->username, "GET_EXAM", "Room not started yet");
        return;
    }

    // Check room FINISHED (status 2 = FINISHED)
    if (status == 2)
    {
        send_error_or_response(client->socket_fd, 225, "ROOM_FINISHED");
        db_log_activity(server->db, "WARNING", client->username, "GET_EXAM", "Room already finished");
        return;
    }

    // Check user in room (participant OR creator)
    int is_participant = db_is_participant(server->db, room_id, client->username);
    int is_creator = db_is_room_creator(server->db, room_id, client->username);

    if (!is_participant && !is_creator)
    {
        send_error_or_response(client->socket_fd, 227, "NOT_IN_ROOM");
        db_log_activity(server->db, "WARNING", client->username, "GET_EXAM", "User not in room");
        return;
    }

    // Get exam questions from DB
    char *exam_json = db_get_exam_questions(server->db, room_id);
    if (!exam_json)
    {
        send_error_or_response(client->socket_fd, 300, "BAD_COMMAND");
        db_log_activity(server->db, "ERROR", client->username, "GET_EXAM", "Failed to get questions");
        return;
    }

    // Parse JSON to extract question_ids and count questions
    int question_count = 0;
    const char *search = exam_json;
    const char *id_start;

    // Reset question arrays
    memset(client->question_ids, 0, sizeof(client->question_ids));
    memset(client->exam_answers, 0, sizeof(client->exam_answers));

    while ((search = strstr(search, "\"question_id\": ")) != NULL && question_count < MAX_QUESTIONS)
    {
        id_start = search + 15; // Move past "\"question_id\": "
        int question_id = atoi(id_start);
        client->question_ids[question_count] = question_id;
        question_count++;
        search = id_start + 1;
    }

    client->exam_total_questions = question_count;

    // Send response: 150 DATA <length>\n<JSON>
    char buffer[1024 * 1024]; // 1MB buffer for exam data
    int len = create_data_message(150, exam_json, strlen(exam_json), buffer, sizeof(buffer));
    if (len > 0)
    {
        send_full(client->socket_fd, buffer, len);
        db_log_activity(server->db, "INFO", client->username, "GET_EXAM", "Success");
    }
    else
    {
        send_error_or_response(client->socket_fd, 300, "BAD_COMMAND");
    }

    free(exam_json);
}

/**
 * @brief Handle SAVE_ANSWER command - Save individual answer incrementally
 * Now uses question_id instead of question_index for correct mapping
 */
void handle_save_answer(Server *server, ClientSession *client, Message *msg)
{
    // Check authentication
    if (!check_authentication(client))
    {
        send_error_or_response(client->socket_fd, CODE_NOT_LOGGED, "NOT_LOGGED");
        return;
    }

    // Validate params: room_id|question_id|option
    if (msg->param_count < 3)
    {
        send_error_or_response(client->socket_fd, CODE_SYNTAX_ERROR, "Usage: SAVE_ANSWER room_id|question_id|option");
        return;
    }

    const char *room_id = msg->params[0];
    int question_id = atoi(msg->params[1]);
    char option = msg->params[2][0];

    // Validate option (A, B, C, or D)
    if (option != 'A' && option != 'B' && option != 'C' && option != 'D')
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS, "Option must be A, B, C, or D");
        return;
    }

    // Check room exists and get status
    int status = db_get_room_status(server->db, room_id);
    if (status < 0)
    {
        send_error_or_response(client->socket_fd, CODE_ROOM_NOT_FOUND, "ROOM_NOT_FOUND");
        return;
    }

    // Check room is IN_PROGRESS (status 1 = IN_PROGRESS)
    if (status != 1)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_STATE, "Room not in progress");
        return;
    }

    // CHECK TIMEOUT: Verify room hasn't exceeded time limit
    int is_expired = db_is_room_expired(server->db, room_id);
    if (is_expired < 0)
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Failed to check room status");
        return;
    }
    if (is_expired)
    {
        send_error_or_response(client->socket_fd, CODE_TIME_EXPIRED, "Room time limit exceeded");
        db_log_activity(server->db, "WARNING", client->username, "SAVE_ANSWER", "Attempted to save after timeout");
        return;
    }

    // Check user in room (participant OR creator)
    int is_participant = db_is_participant(server->db, room_id, client->username);
    int is_creator = db_is_room_creator(server->db, room_id, client->username);

    if (!is_participant && !is_creator)
    {
        send_error_or_response(client->socket_fd, CODE_NOT_IN_ROOM, "NOT_IN_ROOM");
        return;
    }

    // Check user is in exam state
    if (client->state != STATE_IN_EXAM)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_STATE, "Not in exam state");
        return;
    }

    if (client->has_submitted)
    {
        send_error_or_response(client->socket_fd,
                               CODE_INVALID_STATE,
                               "Already submitted");
        return;
    }

    // Check user in correct room
    if (strcmp(client->current_room, room_id) != 0)
    {
        send_error_or_response(client->socket_fd, CODE_NOT_IN_ROOM, "Not in this room");
        return;
    }

    // Find the position of this question_id in the client's question_ids array
    int position = -1;
    for (int i = 0; i < client->exam_total_questions; i++)
    {
        if (client->question_ids[i] == question_id)
        {
            position = i;
            break;
        }
    }

    // Validate question_id (must exist in this exam)
    if (position < 0)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS, "Invalid question ID");
        return;
    }

    // Save answer to buffer using position (allow overwrite)
    client->exam_answers[position] = option;

    // Response success
    send_error_or_response(client->socket_fd, CODE_ANSWER_SAVED, "ANSWER_SAVED");

    // Log activity
    char details[128];
    snprintf(details, sizeof(details), "Saved answer Q%d (ID:%d)=%c", position + 1, question_id, option);
    db_log_activity(server->db, "INFO", client->username, "SAVE_ANSWER", details);
}

/**
 * @brief Handle VIEW_RESULT command
 */
void handle_view_result(Server *server, ClientSession *client, Message *msg)
{
    // check authentication
    if (!check_authentication(client))
    {
        send_error_or_response(client->socket_fd, CODE_NOT_LOGGED, "Not authenticated");
        return;
    }

    // validate params
    if (msg->param_count < 1)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS, "Usage: VIEW_RESULT <room_id>");
        return;
    }

    const char *room_id = msg->params[0];

    // check room status = FINISHED
    int status = db_get_room_status(server->db, room_id);
    if (status == -1)
    {
        send_error_or_response(client->socket_fd, CODE_ROOM_NOT_FOUND, room_id);
        db_log_activity(server->db, "WARNING", client->username, "VIEW_RESULT", "Room not found");
        return;
    }
    else if (status == 0 || status == 1)
    {
        send_error_or_response(client->socket_fd, CODE_ROOM_IN_PROGRESS, room_id);
        db_log_activity(server->db, "WARNING", client->username, "VIEW_RESULT", "Room not finished");
        return;
    }

    // Get leaderboard (returns JSON)
    char *leaderboard_json = db_get_room_leaderboard(server->db, room_id);
    if (!leaderboard_json)
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Failed to get leaderboard");
        db_log_activity(server->db, "ERROR", client->username, "VIEW_RESULT", "Failed to get leaderboard from DB");
        return;
    }

    // send response: 127 DATA <length>\n<JSON leaderboard>
    char buffer[MAX_DATA_SIZE];
    int len = create_data_message(CODE_RESULT_DATA, leaderboard_json, strlen(leaderboard_json), buffer, sizeof(buffer));
    if (len > 0)
    {
        send_full(client->socket_fd, buffer, len);
        db_log_activity(server->db, "INFO", client->username, "VIEW_RESULT", "Viewed results for room");
    }
    else
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Response too large");
    }

    free(leaderboard_json);
    printf("[VIEW_RESULT] User '%s' viewed results for room '%s'\n", client->username, room_id);
}

/**
 * @brief Broadcast message to all participants in room
 */
void broadcast_to_room(Server *server, const char *room_id, const char *message)
{
    pthread_mutex_lock(&server->clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        ClientSession *client = &server->clients[i];
        if (client->active && strcmp(client->current_room, room_id) == 0)
        {
            send_full(client->socket_fd, message, strlen(message));
            printf("  [BROADCAST] Sent to user '%s'\n", client->username);
        }
    }

    pthread_mutex_unlock(&server->clients_mutex);
}

/**
 * @brief Handle START_EXAM command
 */
void handle_start_exam(Server *server, ClientSession *client, Message *msg)
{
    // Check authentication
    if (!check_authentication(client))
    {
        send_error_or_response(client->socket_fd, CODE_NOT_LOGGED, "Not authenticated");
        return;
    }

    // Validate params
    if (msg->param_count < 1)
    {
        send_error_or_response(client->socket_fd, CODE_SYNTAX_ERROR, "Usage: START_EXAM room_id");
        return;
    }

    const char *room_id = msg->params[0];

    int status = db_get_room_status(server->db, room_id);

    // Check if room exists (status < 0 means not found)
    if (status < 0)
    {
        send_error_or_response(client->socket_fd, CODE_ROOM_NOT_FOUND, room_id);
        db_log_activity(server->db, "WARNING", client->username, "START_EXAM", "Room not found");
        return;
    }

    // Check user is creator (after confirming room exists)
    if (!db_is_room_creator(server->db, room_id, client->username))
    {
        send_error_or_response(client->socket_fd, CODE_NOT_CREATOR, room_id);
        db_log_activity(server->db, "WARNING", client->username, "START_EXAM", "Not creator");
        return;
    }

    // Check room not started (status must be 0 = NOT_STARTED)
    if (status != 0) // 1 = IN_PROGRESS, 2 = FINISHED
    {
        if (status == 1)
        {
            send_error_or_response(client->socket_fd, CODE_ROOM_IN_PROGRESS, "Exam is already in progress");
            db_log_activity(server->db, "WARNING", client->username, "START_EXAM", "Room already in progress");
        }
        else if (status == 2)
        {
            send_error_or_response(client->socket_fd, CODE_ROOM_FINISHED, "Exam has already finished");
            db_log_activity(server->db, "WARNING", client->username, "START_EXAM", "Room already finished");
        }
        return;
    }

    // Start room (update status to IN_PROGRESS)
    if (db_start_room(server->db, room_id) < 0)
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Failed to start exam");
        db_log_activity(server->db, "ERROR", client->username, "START_EXAM", "Database error");
        return;
    }

    // Get start time
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", localtime(&now));

    // Prepare broadcast message
    char broadcast_msg[256];
    snprintf(broadcast_msg, sizeof(broadcast_msg), "125 START_OK %s|%s\n", room_id, timestamp);

    // Broadcast to all participants
    printf("[START_EXAM] Broadcasting to room '%s'...\n", room_id);
    broadcast_to_room(server, room_id, broadcast_msg);

    // Update all client sessions in this room to IN_EXAM state
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (server->clients[i].active && strcmp(server->clients[i].current_room, room_id) == 0)
        {
            server->clients[i].state = STATE_IN_EXAM;
            // Initialize exam answer tracking for auto-submit on timeout
            memset(server->clients[i].exam_answers, 0, MAX_QUESTIONS);
            server->clients[i].exam_total_questions = 0;
            server->clients[i].has_submitted = 0;
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);

    // Log activity
    char details[256];
    snprintf(details, sizeof(details), "Exam started at %s", timestamp);
    db_log_activity(server->db, "INFO", client->username, "START_EXAM", details);

    printf("[START_EXAM] Room '%s' started by '%s' at %s\n", room_id, client->username, timestamp);
}

/**
 * @brief Handle SUBMIT_EXAM command
 */
void handle_submit_exam(Server *server, ClientSession *client, Message *msg)
{
    // Check authentication
    if (!check_authentication(client))
    {
        send_error_or_response(client->socket_fd, CODE_NOT_LOGGED, "Not authenticated");
        return;
    }

    // Validate params (room_id only - answers come from client->exam_answers)
    if (msg->param_count < 1)
    {
        send_error_or_response(client->socket_fd, CODE_SYNTAX_ERROR, "Usage: SUBMIT_EXAM room_id");
        return;
    }

    const char *room_id = msg->params[0];

    // Check room exists
    int status = db_get_room_status(server->db, room_id);
    if (status < 0)
    {
        send_error_or_response(client->socket_fd, CODE_ROOM_NOT_FOUND, room_id);
        db_log_activity(server->db, "WARNING", client->username, "SUBMIT_EXAM", "Room not found");
        return;
    }

    // Check room is IN_PROGRESS
    if (status != 1) // 1 = IN_PROGRESS
    {
        if (status == 0)
        {
            send_error_or_response(client->socket_fd, CODE_ROOM_IN_PROGRESS, "Room not started yet");
        }
        else if (status == 2)
        {
            send_error_or_response(client->socket_fd, CODE_ROOM_FINISHED, room_id);
        }
        return;
    }

    // CHECK TIMEOUT: Client cannot submit after deadline
    // (Only force_submit_exam can submit expired exams via cleanup thread)
    int is_expired = db_is_room_expired(server->db, room_id);
    if (is_expired < 0)
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Failed to check room status");
        return;
    }
    if (is_expired)
    {
        send_error_or_response(client->socket_fd, CODE_TIME_EXPIRED, "Exam time expired");
        db_log_activity(server->db, "WARNING", client->username, "SUBMIT_EXAM", "Attempted to submit after timeout");
        return;
    }

    // Check user in room (participant OR creator)
    int is_participant = db_is_participant(server->db, room_id, client->username);
    int is_creator = db_is_room_creator(server->db, room_id, client->username);

    if (!is_participant && !is_creator)
    {
        send_error_or_response(client->socket_fd, CODE_NOT_IN_ROOM, room_id);
        db_log_activity(server->db, "WARNING", client->username, "SUBMIT_EXAM", "Not in room");
        return;
    }

    // Check already submitted
    if (client->has_submitted)
    {
        // Return existing score
        char *result = db_get_exam_result(server->db, room_id, client->username);
        if (result)
        {
            char response[128];
            snprintf(response, sizeof(response), "%s", result);
            send_error_or_response(client->socket_fd, CODE_ALREADY_SUBMITTED, response);
            free(result);
        }
        else
        {
            send_error_or_response(client->socket_fd, CODE_ALREADY_SUBMITTED, "Already submitted");
        }
        db_log_activity(server->db, "WARNING", client->username, "SUBMIT_EXAM", "Already submitted");
        return;
    }

    // Get correct answers and total questions
    int score = 0;
    int total = 0;
    char correct_answers[256]; // Format: "ABCD..."

    if (db_get_correct_answers(server->db, room_id, correct_answers, &total) < 0)
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Failed to grade");
        db_log_activity(server->db, "ERROR", client->username, "SUBMIT_EXAM", "Failed to get correct answers");
        return;
    }

    // Grade from client->exam_answers buffer instead of parsing message
    char answer_string[256] = {0}; // For storing in DB
    int answer_string_pos = 0;

    for (int i = 0; i < total; i++)
    {
        char user_answer = client->exam_answers[i];

        // If unanswered (0), treat as wrong
        if (user_answer == 0)
        {
            user_answer = '-'; // Placeholder for unanswered
        }
        else if (user_answer == correct_answers[i])
        {
            score++;
        }

        // Build answer string for database
        if (i > 0)
        {
            answer_string[answer_string_pos++] = ',';
        }
        answer_string[answer_string_pos++] = user_answer;
    }
    answer_string[answer_string_pos] = '\0';

    // Calculate time taken (in seconds)
    int time_taken = 0;
    time_t start_time = db_get_room_start_time(server->db, room_id);
    if (start_time > 0)
    {
        time_t now = time(NULL);
        time_taken = (int)(now - start_time);
    }

    // Save result to database
    if (db_submit_exam(server->db, room_id, client->username, score, total, answer_string, time_taken) < 0)
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Failed to save result");
        db_log_activity(server->db, "ERROR", client->username, "SUBMIT_EXAM", "Database error");
        return;
    }

    // Mark as submitted
    client->has_submitted = 1;

    // Send response: 130 SUBMIT_OK score|total
    char response[128];
    snprintf(response, sizeof(response), "%d|%d", score, total);
    send_error_or_response(client->socket_fd, CODE_SUBMIT_OK, response);

    // Update client state
    client->state = STATE_AUTHENTICATED;
    memset(client->current_room, 0, sizeof(client->current_room));

    // Log activity
    char details[256];
    snprintf(details, sizeof(details), "Score: %d/%d", score, total);
    db_log_activity(server->db, "INFO", client->username, "SUBMIT_EXAM", details);

    printf("[SUBMIT_EXAM] User '%s' scored %d/%d in room '%s'\n", client->username, score, total, room_id);

    // Check if all participants have submitted
    if (db_check_all_submitted(server->db, room_id))
    {
        // Auto-finish the room
        if (db_finish_room(server->db, room_id) == 0)
        {
            printf("[AUTO-FINISH] Room '%s' finished - all participants submitted\n", room_id);
            db_log_activity(server->db, "INFO", "SYSTEM", "AUTO_FINISH_ROOM", room_id);
        }
    }
}

/**
 * @brief Force submit exam for a client (used by timeout auto-submit)
 * This is an internal function called by the cleanup thread
 */
int force_submit_exam(Server *server, ClientSession *client, const char *room_id)
{
    // Skip if already submitted
    if (client->has_submitted)
    {
        return 0; // Already submitted, nothing to do
    }

    // Get correct answers and total questions
    int score = 0;
    int total = 0;
    char correct_answers[256];

    if (db_get_correct_answers(server->db, room_id, correct_answers, &total) < 0)
    {
        printf("[FORCE_SUBMIT] Failed to get correct answers for room '%s'\n", room_id);
        return -1;
    }

    // Grade from client->exam_answers buffer
    char answer_string[256] = {0};
    int answer_string_pos = 0;

    for (int i = 0; i < total; i++)
    {
        char user_answer = client->exam_answers[i];

        // If unanswered (0), treat as wrong
        if (user_answer == 0)
        {
            user_answer = '-'; // Placeholder for unanswered
        }
        else if (user_answer == correct_answers[i])
        {
            score++;
        }

        // Build answer string for database
        if (i > 0)
        {
            answer_string[answer_string_pos++] = ',';
        }
        answer_string[answer_string_pos++] = user_answer;
    }
    answer_string[answer_string_pos] = '\0';

    // Calculate time taken (in seconds)
    int time_taken = 0;
    time_t start_time = db_get_room_start_time(server->db, room_id);
    if (start_time > 0)
    {
        time_t now = time(NULL);
        time_taken = (int)(now - start_time);
    }

    // Save result to database
    if (db_submit_exam(server->db, room_id, client->username, score, total, answer_string, time_taken) < 0)
    {
        printf("[FORCE_SUBMIT] Failed to save result for user '%s' in room '%s'\n", client->username, room_id);
        return -1;
    }

    // Mark as submitted
    client->has_submitted = 1;

    // Update client state
    client->state = STATE_AUTHENTICATED;
    memset(client->current_room, 0, sizeof(client->current_room));

    // Log activity
    char details[256];
    snprintf(details, sizeof(details), "AUTO_SUBMIT (timeout): Score %d/%d", score, total);
    db_log_activity(server->db, "INFO", client->username, "FORCE_SUBMIT", details);

    printf("[FORCE_SUBMIT] User '%s' auto-submitted: %d/%d in room '%s'\n",
           client->username, score, total, room_id);

    return 0;
}
