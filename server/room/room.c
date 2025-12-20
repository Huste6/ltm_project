#include "room.h"
#include "../server.h"
#include "../auth/auth.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAX_PARTICIPANTS 50

/**
 * @brief Handle CREATE_ROOM command
 */
void handle_create_room(Server *server, ClientSession *client, Message *msg)
{
    // Check authentication
    if (!check_authentication(client))
    {
        send_error_or_response(client->socket_fd, CODE_NOT_LOGGED, "Not authenticated");
        return;
    }

    // Validate params: must have exactly 3 params
    if (msg->param_count < 3)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS, "Usage: CREATE_ROOM <room_name> <num_questions> <time_limit_minutes>");
        return;
    }

    const char *room_name = msg->params[0];
    const char *num_questions_str = msg->params[1];
    const char *time_limit_str = msg->params[2];

    // Check for empty params
    if (strlen(room_name) == 0 || strlen(num_questions_str) == 0 || strlen(time_limit_str) == 0)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS, "All parameters must be non-empty");
        return;
    }

    // Parse integer params to validate
    int num_questions = atoi(num_questions_str);
    int time_limit = atoi(time_limit_str);

    // Validate num_questions: must be integer > 0
    if (num_questions <= 0)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS, "num_questions must be a positive integer");
        db_log_activity(server->db, "WARNING", client->username, "CREATE_ROOM", "Invalid num_questions: must be > 0");
        return;
    }

    // Validate time_limit: must be integer > 0
    if (time_limit <= 0)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS, "time_limit_minutes must be a positive integer");
        db_log_activity(server->db, "WARNING", client->username, "CREATE_ROOM", "Invalid time_limit: must be > 0");
        return;
    }

    // Generate unique room ID
    char room_id[MAX_ROOM_ID_LEN];
    snprintf(room_id, sizeof(room_id), "%ld", time(NULL)); // Use timestamp as room ID

    // Create room in database (stored procedure handles question selection)
    if (db_create_room(server->db, room_id, room_name, client->username, num_questions, time_limit) < 0)
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Failed to create room");
        db_log_activity(server->db, "ERROR", client->username, "CREATE_ROOM", "Database error");
        return;
    }

    // Update client session
    strcpy(client->current_room, room_id);
    client->state = STATE_IN_ROOM;

    // Send success response: 120 ROOM_CREATED <room_id>
    send_error_or_response(client->socket_fd, CODE_ROOM_CREATED, room_id);

    // Log activity
    char details[256];
    snprintf(details, sizeof(details), "Room '%s' (%s): %d questions, %d min", room_name, room_id, num_questions, time_limit);
    db_log_activity(server->db, "INFO", client->username, "CREATE_ROOM", details);

    printf("[CREATE_ROOM] User '%s' created room '%s' (%s)\n", client->username, room_name, room_id);
}

/**
 * @brief Handle LIST_ROOMS command
 */
void handle_list_rooms(Server *server, ClientSession *client, Message *msg)
{
    // Check authentication
    if (!check_authentication(client))
    {
        send_error_or_response(client->socket_fd, CODE_NOT_LOGGED, "Not authenticated");
        return;
    }

    // Get filter (default: ALL)
    const char *filter = msg->param_count > 0 ? msg->params[0] : "ALL";

    // Validate filter
    if (strcmp(filter, "ALL") != 0 &&
        strcmp(filter, "NOT_STARTED") != 0 &&
        strcmp(filter, "IN_PROGRESS") != 0 &&
        strcmp(filter, "FINISHED") != 0)
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS, "Filter must be: ALL, NOT_STARTED, IN_PROGRESS, or FINISHED");
        return;
    }

    // Get room list from database (returns JSON string)
    char *json_data = db_list_rooms(server->db, filter);
    if (!json_data)
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Failed to fetch rooms");
        db_log_activity(server->db, "ERROR", client->username, "LIST_ROOMS", "Database error");
        return;
    }

    // Send response with length prefixing
    char buffer[MAX_MESSAGE_LEN];
    int len = create_data_message(CODE_ROOMS_DATA, json_data, strlen(json_data), buffer, sizeof(buffer));

    if (len > 0)
    {
        send_full(client->socket_fd, buffer, len);
        db_log_activity(server->db, "INFO", client->username, "LIST_ROOMS", filter);
    }
    else
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Response too large");
    }

    free(json_data);

    printf("[LIST_ROOMS] User '%s' requested room list (filter: %s)\n", client->username, filter);
}

/**
 * @brief Handle JOIN_ROOM command
 */
void handle_join_room(Server *server, ClientSession *client, Message *msg)
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
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS, "Usage: JOIN_ROOM room_id");
        return;
    }

    const char *room_id = msg->params[0];

    // Check room exists
    int status = db_get_room_status(server->db, room_id);
    if (status < 0)
    {
        send_error_or_response(client->socket_fd, CODE_ROOM_NOT_FOUND, room_id);
        db_log_activity(server->db, "WARNING", client->username, "JOIN_ROOM", "Room not found");
        return;
    }

    // Check room not started (status: 0=NOT_STARTED, 1=IN_PROGRESS, 2=FINISHED)
    if (status == 1)
    {
        send_error_or_response(client->socket_fd, CODE_ROOM_IN_PROGRESS, room_id);
        db_log_activity(server->db, "WARNING", client->username, "JOIN_ROOM", "Room already started");
        return;
    }

    if (status == 2)
    {
        send_error_or_response(client->socket_fd, CODE_ROOM_FINISHED, room_id);
        db_log_activity(server->db, "WARNING", client->username, "JOIN_ROOM", "Room finished");
        return;
    }

    // Check room not full
    int participant_count = db_get_room_participant_count(server->db, room_id);
    if (participant_count >= MAX_PARTICIPANTS)
    {
        send_error_or_response(client->socket_fd, CODE_ROOM_FULL, room_id);
        db_log_activity(server->db, "WARNING", client->username, "JOIN_ROOM", "Room full");
        return;
    }

    // Join room
    if (db_join_room(server->db, room_id, client->username) < 0)
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Failed to join room");
        db_log_activity(server->db, "ERROR", client->username, "JOIN_ROOM", "Database error");
        return;
    }

    // Update client session
    strcpy(client->current_room, room_id);
    client->state = STATE_IN_ROOM;

    // Send success response
    send_error_or_response(client->socket_fd, CODE_ROOM_JOIN_OK, room_id);

    db_log_activity(server->db, "INFO", client->username, "JOIN_ROOM", room_id);

    printf("[JOIN_ROOM] User '%s' joined room '%s'\n", client->username, room_id);
}

/**
 * @brief Handle LEAVE_ROOM command
 */
void handle_leave_room(Server *server, ClientSession *client, Message *msg)
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
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS,
                               "Usage: LEAVE_ROOM room_id");
        return;
    }

    const char *room_id = msg->params[0];

    // Check user is in room
    if (!db_is_in_room(server->db, room_id, client->username))
    {
        send_error_or_response(client->socket_fd, CODE_NOT_IN_ROOM, room_id);
        db_log_activity(server->db, "WARNING", client->username,
                        "LEAVE_ROOM", "Not in room");
        return;
    }

    // Leave room
    if (db_leave_room(server->db, room_id, client->username) < 0)
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR,
                               "Failed to leave room");
        db_log_activity(server->db, "ERROR", client->username,
                        "LEAVE_ROOM", "Database error");
        return;
    }

    // Update client session
    memset(client->current_room, 0, sizeof(client->current_room));
    client->state = STATE_AUTHENTICATED;

    // Send success response
    send_error_or_response(client->socket_fd, CODE_ROOM_LEAVE_OK, room_id);

    db_log_activity(server->db, "INFO", client->username,
                    "LEAVE_ROOM", room_id);

    printf("[LEAVE_ROOM] User '%s' left room '%s'\n",
           client->username, room_id);
}

/**
 * @brief Handle FINISH_EXAM command - transition room from IN_PROGRESS to FINISHED
 */
void handle_finish_exam(Server *server, ClientSession *client, Message *msg)
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
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS,
                               "Usage: FINISH_EXAM room_id");
        return;
    }

    const char *room_id = msg->params[0];

    // Check user is room creator
    if (!db_is_room_creator(&server->db, room_id, client->username))
    {
        send_error_or_response(client->socket_fd, CODE_NOT_ALLOWED,
                               "Only room creator can finish exam");
        db_log_activity(server->db, "WARNING", client->username,
                        "FINISH_EXAM", "Not creator of room");
        return;
    }

    // Check room exists and status = IN_PROGRESS
    int status = db_get_room_status(&server->db, room_id);
    if (status < 0)
    {
        send_error_or_response(client->socket_fd, CODE_ROOM_NOT_FOUND, room_id);
        db_log_activity(server->db, "WARNING", client->username,
                        "FINISH_EXAM", "Room not found");
        return;
    }

    if (status != 1) // 1 = IN_PROGRESS
    {
        send_error_or_response(client->socket_fd, CODE_INVALID_STATE,
                               "Room must be IN_PROGRESS to finish exam");
        db_log_activity(server->db, "WARNING", client->username,
                        "FINISH_EXAM", "Room not in progress");
        return;
    }

    // Finish room (update status to FINISHED)
    if (db_finish_room(&server->db, room_id) < 0)
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR,
                               "Failed to finish exam");
        db_log_activity(server->db, "ERROR", client->username,
                        "FINISH_EXAM", "Database error");
        return;
    }

    // Send success response
    send_error_or_response(client->socket_fd, CODE_EXAM_FINISHED, room_id);

    db_log_activity(server->db, "INFO", client->username,
                    "FINISH_EXAM", room_id);

    printf("[FINISH_EXAM] User '%s' finished exam in room '%s'\n",
           client->username, room_id);
}