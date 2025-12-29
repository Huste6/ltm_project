#include "exam.h"
#include "../server.h"
#include "../auth/auth.h"
#include "../practice/practice.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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
    const char *search = exam_json; // pointer for searching, scan through JSON
    const char *id_start;

    // Reset question arrays, make sure no stale data
    memset(client->question_ids, 0, sizeof(client->question_ids));
    memset(client->exam_answers, 0, sizeof(client->exam_answers));

    // Extract question_ids from JSON to map to client's answer buffer, serve when client MODIFY answer
    // Index of the array corresponds to question order in exam
    while ((search = strstr(search, "\"question_id\": ")) != NULL && question_count < MAX_QUESTIONS)
    {
        id_start = search + 15; // Move past "\"question_id\": "
        int question_id = atoi(id_start);
        client->question_ids[question_count] = question_id; // Store question_id in client's array
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
        send_error_or_response(client->socket_fd, CODE_NOT_IN_PROGRESS, "Room not in progress");
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
        send_error_or_response(client->socket_fd, CODE_NOT_IN_PROGRESS, "Not in exam state");
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
 * @brief Cleanup thread for timed out rooms and practice sessions
 * Runs every 10 seconds to check for expired rooms/practices and auto-submit
 */
void *cleanup_timed_out_rooms(void *arg)
{
    Server *server = (Server *)arg;
    while (server->running)
    {
        sleep(10); // Check every 10 seconds

        // ========== EXAM ROOMS CLEANUP ==========
        // Check and finish timed out rooms
        int finished_count = db_check_and_finish_timed_out_rooms(server->db);

        if (finished_count > 0)
        {
            printf("[CLEANUP_THREAD] Found %d timed-out room(s), processing auto-submit...\n", finished_count);

            // Get list of rooms that just finished
            // For each finished room, force-submit all clients who haven't submitted
            pthread_mutex_lock(&server->clients_mutex);

            // Track which rooms we've processed to broadcast once per room
            char processed_rooms[MAX_CLIENTS][32];
            int processed_count = 0;

            // STEP 1: Force submit ALL users first (before broadcasting)
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                ClientSession *client = &server->clients[i];

                // Skip inactive clients or clients not in exam
                if (!client->active || client->state != STATE_IN_EXAM)
                    continue;

                // Check if client's room is finished (timed out)
                int status = db_get_room_status(server->db, client->current_room);
                if (status == 2) // FINISHED
                {
                    // Track rooms we need to broadcast to
                    int already_tracked = 0;
                    for (int j = 0; j < processed_count; j++)
                    {
                        if (strcmp(processed_rooms[j], client->current_room) == 0)
                        {
                            already_tracked = 1;
                            break;
                        }
                    }

                    if (!already_tracked && processed_count < MAX_CLIENTS)
                    {
                        strncpy(processed_rooms[processed_count], client->current_room, 31);
                        processed_rooms[processed_count][31] = '\0';
                        processed_count++;
                    }

                    // Force submit if not already submitted
                    if (!client->has_submitted)
                    {
                        printf("[CLEANUP_THREAD] Auto-submitting for user '%s' in room '%s'\n",
                               client->username, client->current_room);

                        force_submit_exam(server, client, client->current_room);
                    }
                }
            }

            // STEP 2: After ALL submissions complete, broadcast TIME_EXPIRED
            for (int i = 0; i < processed_count; i++)
            {
                char timeout_msg[128];
                snprintf(timeout_msg, sizeof(timeout_msg), "230 TIME_EXPIRED %s\n", processed_rooms[i]);

                printf("[CLEANUP_THREAD] Broadcasting timeout to room '%s'\n", processed_rooms[i]);
                broadcast_to_room(server, processed_rooms[i], timeout_msg);
            }

            pthread_mutex_unlock(&server->clients_mutex);

            printf("[CLEANUP_THREAD] Finished %d room(s) due to timeout\n", finished_count);
        }

        // ========== PRACTICE SESSIONS CLEANUP ==========
        pthread_mutex_lock(&server->clients_mutex);

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            ClientSession *client = &server->clients[i];

            // Skip inactive clients or clients not in practice
            if (!client->active || client->state != STATE_IN_PRACTICE)
                continue;

            // Check if practice session has timed out
            if (strlen(client->practice_id) == 0)
                continue;

            char session_username[MAX_USERNAME_LEN];
            time_t start_time;
            int time_limit;

            if (db_get_practice_info(server->db, client->practice_id,
                                     session_username, &start_time, &time_limit) < 0)
            {
                continue;
            }

            time_t now = time(NULL);
            time_t elapsed = now - start_time;
            time_t time_limit_seconds = time_limit * 60;

            if (elapsed > time_limit_seconds)
            {
                // Practice has timed out - force submit and send result with timeout flag
                printf("[CLEANUP_THREAD] Practice '%s' timed out, auto-submitting for user '%s'\n",
                       client->practice_id, client->username);

                // Get correct answers to calculate score
                int score = 0;
                int total = 0;
                char correct_answers[256];

                if (db_get_practice_answers(server->db, client->practice_id,
                                            correct_answers, &total) >= 0)
                {
                    // Grade from client->practice_answers buffer
                    for (int j = 0; j < total; j++)
                    {
                        char user_answer = client->practice_answers[j];
                        if (user_answer != 0 && user_answer == correct_answers[j])
                        {
                            score++;
                        }
                    }
                }

                force_submit_practice(server, client, client->practice_id);

                // Send result with TIMEOUT flag to client
                char result[256];
                snprintf(result, sizeof(result), "%d|%d|TIMEOUT", score, total);
                send_error_or_response(client->socket_fd, CODE_PRACTICE_RESULT, result);
            }
        }

        pthread_mutex_unlock(&server->clients_mutex);
    }

    printf("[CLEANUP_THREAD] Stopped\n");
    return NULL;
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
            memset(server->clients[i].exam_answers, 0, MAX_QUESTIONS); // No answers yet
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

    // Check already submitted FIRST (before checking room status)
    // This allows users to see results even after room is finished
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

    // Check room exists
    int status = db_get_room_status(server->db, room_id);
    if (status < 0)
    {
        send_error_or_response(client->socket_fd, CODE_ROOM_NOT_FOUND, room_id);
        db_log_activity(server->db, "WARNING", client->username, "SUBMIT_EXAM", "Room not found");
        return;
    }

    // Check room is IN_PROGRESS or FINISHED (allow submit after timeout for force-submit)
    if (status != 1 && status != 2) // 1 = IN_PROGRESS, 2 = FINISHED
    {
        if (status == 0)
        {
            send_error_or_response(client->socket_fd, CODE_ROOM_IN_PROGRESS, "Room not started yet");
        }
        return;
    }

    // If room is FINISHED and user hasn't submitted, do force-submit now
    if (status == 2) // FINISHED
    {
        printf("[SUBMIT_EXAM] Room finished, force-submitting for user '%s' in room '%s'\n",
               client->username, room_id);

        // Call force_submit_exam which will grade and save
        if (force_submit_exam(server, client, room_id) == 0)
        {
            // Now retrieve and return the result
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
                send_error_or_response(client->socket_fd, CODE_ALREADY_SUBMITTED, "Exam submitted (timeout)");
            }
        }
        else
        {
            send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Failed to submit exam");
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
