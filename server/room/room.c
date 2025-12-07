#include "room.h"
#include "../server.h"
#include "../protocol/protocol.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/**
 * @brief Trim whitespace from string
 * @param str Input string to trim
 * - Trims leading and trailing whitespace characters to clean up input
 * (because user input may have extra spaces, cause error while using strcmp)
 */
static void trim(char *str)
{
    if (!str)
        return;

    // Trim leading
    char *start = str;
    while (*start && isspace(*start))
        start++;

    // Trim trailing
    char *end = start + strlen(start) - 1;
    while (end > start && isspace(*end))
        end--;
    *(end + 1) = '\0';

    // Move to start
    if (start != str)
    {
        memmove(str, start, strlen(start) + 1);
    }
}

/**
 * @brief Validate room status filter
 * @return 1 if valid, 0 if invalid
 */
static int is_valid_status(const char *status)
{
    if (!status || strlen(status) == 0)
        return 1; // Empty is valid (no filter)
    return (strcmp(status, "NOT_STARTED") == 0 ||
            strcmp(status, "IN_PROGRESS") == 0 ||
            strcmp(status, "FINISHED") == 0);
}

/**
 * @brief Handle LIST_ROOMS command
 */
void handle_list_rooms(Server *server, ClientSession *client, Message *msg)
{
    char status_filter[32] = {0};

    // Extract and validate filter parameter
    if (msg->param_count > 0 && strlen(msg->params[0]) > 0)
    {
        strncpy(status_filter, msg->params[0], sizeof(status_filter) - 1);
        trim(status_filter);

        // Validate filter
        if (!is_valid_status(status_filter))
        {
            send_error_or_response(client->socket_fd, CODE_INVALID_PARAMS, "Invalid status filter. Use: NOT_STARTED|IN_PROGRESS|FINISHED");
            db_log_activity(server->db, "WARNING", client->username, "LIST_ROOMS", "Invalid status filter");
            return;
        }
    }

    // Get rooms JSON from database
    char *json_data = NULL;
    const char *filter_to_use = (strlen(status_filter) > 0) ? status_filter : NULL;

    if (db_get_rooms_json(server->db, filter_to_use, &json_data) != 0)
    {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Failed to retrieve rooms");
        db_log_activity(server->db, "ERROR", client->username, "LIST_ROOMS", "Database query failed");
        return;
    }

    // Send response: 121 DATA <length>\n<json>
    size_t json_len = strlen(json_data);
    char header[256];
    snprintf(header, sizeof(header), "%d DATA %zu\n", CODE_ROOMS_DATA, json_len);

    // Send header
    if (send_full(client->socket_fd, header, strlen(header)) < 0)
    {
        fprintf(stderr, "[LIST_ROOMS] Failed to send header to client %d\n", client->socket_fd);
        free(json_data);
        return;
    }

    // Send JSON body
    if (send_full(client->socket_fd, json_data, json_len) < 0)
    {
        fprintf(stderr, "[LIST_ROOMS] Failed to send JSON data to client %d\n", client->socket_fd);
        free(json_data);
        return;
    }

    // Log success
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Listed rooms (filter: %s, count: calculated from JSON)",
             filter_to_use ? filter_to_use : "none");
    db_log_activity(server->db, "INFO", client->username, "LIST_ROOMS", log_msg);

    printf("[LIST_ROOMS] User '%s' listed rooms (filter: %s, size: %zu bytes)\n",
           client->username, filter_to_use ? filter_to_use : "none", json_len);

    free(json_data);
}

/**
 * @brief Handle CREATE_ROOM command (stub)
 */
void handle_create_room(Server *server, ClientSession *client, Message *msg)
{
    (void)server;
    (void)msg;
    send_error_or_response(client->socket_fd, CODE_BAD_COMMAND, "CREATE_ROOM not implemented yet");
}

/**
 * @brief Handle JOIN_ROOM command (stub)
 */
void handle_join_room(Server *server, ClientSession *client, Message *msg)
{
    (void)server;
    (void)msg;
    send_error_or_response(client->socket_fd, CODE_BAD_COMMAND, "JOIN_ROOM not implemented yet");
}

/**
 * @brief Handle LEAVE_ROOM command (stub)
 */
void handle_leave_room(Server *server, ClientSession *client, Message *msg)
{
    (void)server;
    (void)msg;
    send_error_or_response(client->socket_fd, CODE_BAD_COMMAND, "LEAVE_ROOM not implemented yet");
}

/**
 * @brief Handle START_EXAM command (stub)
 */
void handle_start_exam(Server *server, ClientSession *client, Message *msg)
{
    (void)server;
    (void)msg;
    send_error_or_response(client->socket_fd, CODE_BAD_COMMAND, "START_EXAM not implemented yet");
}
