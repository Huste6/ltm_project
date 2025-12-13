#include "room.h"
#include "../server.h"
#include "../auth/auth.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/**
 * @brief Handle CREATE_ROOM command
 */
void handle_create_room(Server *server, ClientSession *client, Message *msg)
{
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
        send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS,
                               "Filter must be: ALL, NOT_STARTED, IN_PROGRESS, or FINISHED");
        return;
    }

    // Get room list from database (returns JSON string)
    char *json_data = db_list_rooms(server->db, filter);
    if (!json_data)
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR,
                               "Failed to fetch rooms");
        db_log_activity(server->db, "ERROR", client->username,
                        "LIST_ROOMS", "Database error");
        return;
    }

    // Send response with length prefixing
    char buffer[MAX_MESSAGE_LEN];
    int len = create_data_message(CODE_ROOMS_DATA, json_data, strlen(json_data),
                                  buffer, sizeof(buffer));

    if (len > 0)
    {
        send_full(client->socket_fd, buffer, len);
        db_log_activity(server->db, "INFO", client->username,
                        "LIST_ROOMS", filter);
    }
    else
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR,
                               "Response too large");
    }

    free(json_data);

    printf("[LIST_ROOMS] User '%s' requested room list (filter: %s)\n",
           client->username, filter);
}

/**
 * @brief Handle JOIN_ROOM command
 */
void handle_join_room(Server *server, ClientSession *client, Message *msg)
{
}

/**
 * @brief Handle LEAVE_ROOM command
 */
void handle_leave_room(Server *server, ClientSession *client, Message *msg)
{
}