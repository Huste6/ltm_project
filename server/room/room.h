#ifndef ROOM_H
#define ROOM_H

#include "../server.h"
#include "../database/database.h"
#include "../protocol/protocol.h"

/**
 * @brief Handle LIST_ROOMS command
 * @param server Server instance
 * @param client Client session
 * @param msg Parsed message with optional filter parameter
 *
 * Format: LIST_ROOMS [filter]\n
 * Filter: NOT_STARTED | IN_PROGRESS | FINISHED (optional)
 *
 * Response: 121 DATA <length>\n<json>
 * JSON format: {"rooms":[{room_id,name,creator,status,participants,num_questions,time_limit,created_at}, ...]}
 */
void handle_list_rooms(Server *server, ClientSession *client, Message *msg);

/**
 * @brief Handle CREATE_ROOM command
 */
void handle_create_room(Server *server, ClientSession *client, Message *msg);

/**
 * @brief Handle JOIN_ROOM command
 */
void handle_join_room(Server *server, ClientSession *client, Message *msg);

/**
 * @brief Handle LEAVE_ROOM command
 */
void handle_leave_room(Server *server, ClientSession *client, Message *msg);

/**
 * @brief Handle START_EXAM command
 */
void handle_start_exam(Server *server, ClientSession *client, Message *msg);

#endif // ROOM_H
