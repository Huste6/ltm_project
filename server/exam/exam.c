#include "exam.h"
#include "../server.h"
#include "../auth/auth.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

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
        send_error_or_response(client->socket_fd, CODE_ROOM_NOT_FINISHED, room_id);
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
