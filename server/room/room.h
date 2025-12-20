#ifndef ROOM_H
#define ROOM_H

#include "../protocol/protocol.h"
#include "../database/database.h"

typedef struct ClientSession ClientSession;
typedef struct Server Server;

/**
 * @brief Create a new room
 * @param server Pointer to Server instance
 * @param client Pointer to ClientSession
 * @param msg Message parsed (CREATE_ROOM room_name num_questions time_limit)
 * Flow:
 * 1. Check authentication
 * 2. Validate parameters
 * 3. Create room in database
 * 4. Update client state
 * 5. Response: 120 ROOM_CREATED <room_id>
 */
void handle_create_room(Server *server, ClientSession *client, Message *msg);

/**
 * @brief Handle LIST_ROOMS command
 * @param server Pointer to Server instance
 * @param client Pointer to ClientSession
 * @param msg Message parsed (LIST_ROOMS [filter])
 * Flow:
 * 1. Check authentication
 * 2. Validate filter parameter
 * 3. Query database for room list
 * Filter options: ALL, NOT_STARTED, IN_PROGRESS, FINISHED
 * Response: 121 DATA <length>\n<JSON>
 */
void handle_list_rooms(Server *server, ClientSession *client, Message *msg);

/**
 * @brief Handle joining a room
 * @param server Pointer to Server instance
 * @param client Pointer to ClientSession
 * @param msg Message parsed (JOIN_ROOM room_id)
 *
 * Flow:
 * 1. Check authentication
 * 2. Validate room exists
 * 3. Check room status = NOT_STARTED
 * 4. Check room not full
 * 5. Add user to participants
 * 6. Update client state
 * 7. Response: 122 ROOM_JOIN_OK <room_id>
 */
void handle_join_room(Server *server, ClientSession *client, Message *msg);

/**
 * @brief Handle leaving a room
 * @param server Pointer to Server instance
 * @param client Pointer to ClientSession
 * @param msg Message parsed (LEAVE_ROOM room_id)
 *
 * Flow:
 * 1. Check user in room
 * 2. Remove from participants
 * 3. Update client state
 * 4. Response: 123 ROOM_LEAVE_OK
 */
void handle_leave_room(Server *server, ClientSession *client, Message *msg);

/**
 * @brief Handle finishing an exam in a room
 * @param server Pointer to Server instance
 * @param client Pointer to ClientSession
 * @param msg Message parsed (FINISH_EXAM room_id)
 *
 * Flow:
 * 1. Check authentication
 * 2. Check user is creator of room
 * 3. Update room status to FINISHED
 * 4. Calculate scores for all participants
 * 5. Response: 125 EXAM_FINISHED
 */
void handle_finish_exam(Server *server, ClientSession *client, Message *msg);

#endif // ROOM_H