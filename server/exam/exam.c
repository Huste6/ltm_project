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
    // 1. Check authentication
    if (!check_authentication(client))
    {
        send_error_or_response(client->socket_fd, 221, "NOT_LOGGED");
        return;
    }

    // 2. Validate params
    if (msg->param_count < 1)
    {
        send_error_or_response(client->socket_fd, 300, "BAD_COMMAND");
        return;
    }

    const char *room_id = msg->params[0];

    // 3. Check room exists
    int status = db_get_room_status(server->db, room_id);
    if (status < 0)
    {
        send_error_or_response(client->socket_fd, 223, "ROOM_NOT_FOUND");
        db_log_activity(server->db, "WARNING", client->username, "GET_EXAM", "Room not found");
        return;
    }

    // 4. Check room NOT_STARTED (status 0 = NOT_STARTED)
    if (status == 0)
    {
        send_error_or_response(client->socket_fd, 224, "ROOM_NOT_STARTED");
        db_log_activity(server->db, "WARNING", client->username, "GET_EXAM", "Room not started yet");
        return;
    }

    // 5. Check room FINISHED (status 2 = FINISHED)
    if (status == 2)
    {
        send_error_or_response(client->socket_fd, 225, "ROOM_FINISHED");
        db_log_activity(server->db, "WARNING", client->username, "GET_EXAM", "Room already finished");
        return;
    }

    // 6. Check user in room
    if (!db_is_in_room(server->db, client->username, room_id))
    {
        send_error_or_response(client->socket_fd, 227, "NOT_IN_ROOM");
        db_log_activity(server->db, "WARNING", client->username, "GET_EXAM", "User not in room");
        return;
    }

    // 7. Get exam questions from DB
    char *exam_json = db_get_exam_questions(server->db, room_id);
    if (!exam_json)
    {
        send_error_or_response(client->socket_fd, 300, "BAD_COMMAND");
        db_log_activity(server->db, "ERROR", client->username, "GET_EXAM", "Failed to get questions");
        return;
    }

    // 8. Send response: 150 DATA <length>\n<JSON>
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
    if (status != 2)
    {
        send_error_or_response(client->socket_fd, CODE_ROOM_FINISHED, room_id);
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
// ==========================================
#include "exam.h"
#include "../server.h"
#include "../auth/auth.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

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
        send_error_or_response(client->socket_fd, CODE_SYNTAX_ERROR,
                               "Usage: START_EXAM room_id");
        return;
    }

    const char *room_id = msg->params[0];

    // Check room status FIRST (to verify room exists)
    int status = db_get_room_status(server->db, room_id);

    // Check if room exists (status < 0 means not found)
    if (status < 0)
    {
        send_error_or_response(client->socket_fd, CODE_ROOM_NOT_FOUND, room_id);
        db_log_activity(server->db, "WARNING", client->username,
                        "START_EXAM", "Room not found");
        return;
    }

    // Check user is creator (after confirming room exists)
    if (!db_is_room_creator(server->db, room_id, client->username))
    {
        send_error_or_response(client->socket_fd, CODE_NOT_CREATOR, room_id);
        db_log_activity(server->db, "WARNING", client->username,
                        "START_EXAM", "Not creator");
        return;
    }

    // Check room not started (status must be 0 = NOT_STARTED)
    if (status != 0) // 1 = IN_PROGRESS, 2 = FINISHED
    {
        send_error_or_response(client->socket_fd, CODE_ROOM_ALREADY_STARTED, room_id);
        db_log_activity(server->db, "WARNING", client->username,
                        "START_EXAM", "Room already started");
        return;
    }

    // Start room (update status to IN_PROGRESS)
    if (db_start_room(server->db, room_id) < 0)
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR,
                               "Failed to start exam");
        db_log_activity(server->db, "ERROR", client->username,
                        "START_EXAM", "Database error");
        return;
    }

    // Get start time
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S",
             localtime(&now));

    // Prepare broadcast message
    char broadcast_msg[256];
    snprintf(broadcast_msg, sizeof(broadcast_msg), "125 START_OK %s|%s\n",
             room_id, timestamp);

    // Broadcast to all participants
    printf("[START_EXAM] Broadcasting to room '%s'...\n", room_id);
    broadcast_to_room(server, room_id, broadcast_msg);

    // Update all client sessions in this room to IN_EXAM state
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (server->clients[i].active &&
            strcmp(server->clients[i].current_room, room_id) == 0)
        {
            server->clients[i].state = STATE_IN_EXAM;
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);

    // Log activity
    char details[256];
    snprintf(details, sizeof(details), "Exam started at %s", timestamp);
    db_log_activity(server->db, "INFO", client->username,
                    "START_EXAM", details);

    printf("[START_EXAM] Room '%s' started by '%s' at %s\n",
           room_id, client->username, timestamp);
}
