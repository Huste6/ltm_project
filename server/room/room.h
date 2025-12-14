#ifndef ROOM_H
#define ROOM_H

#include "../protocol/protocol.h"
#include "../database/database.h"

typedef struct ClientSession ClientSession;
typedef struct Server Server;

/**
 * @brief Xử lý tạo phòng thi mới
 * @param server Pointer tới Server instance
 * @param client Pointer tới ClientSession
 * @param msg Message đã parse (CREATE_ROOM name|num_questions|time_limit)
 *
 * Flow:
 * 1. Check authentication
 * 2. Validate params (num_questions: 5-50, time: 5-120)
 * 3. Generate unique room_id (timestamp-based)
 * 4. Create room trong DB (với random questions)
 * 5. Creator tự động join
 * 6. Update client state
 * 7. Response: 120 ROOM_CREATED <room_id>
 */
void handle_create_room(Server *server, ClientSession *client, Message *msg);

/**
 * @brief Xử lý xem danh sách phòng thi
 * @param server Pointer tới Server instance
 * @param client Pointer tới ClientSession
 * @param msg Message đã parse (LIST_ROOMS [filter])
 *
 * Filter options: ALL, NOT_STARTED, IN_PROGRESS, FINISHED
 * Response: 121 DATA <length>\n<JSON>
 */
void handle_list_rooms(Server *server, ClientSession *client, Message *msg);

/**
 * @brief Xử lý tham gia phòng thi
 * @param server Pointer tới Server instance
 * @param client Pointer tới ClientSession
 * @param msg Message đã parse (JOIN_ROOM room_id)
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
 * @brief Xử lý rời phòng thi
 * @param server Pointer tới Server instance
 * @param client Pointer tới ClientSession
 * @param msg Message đã parse (LEAVE_ROOM room_id)
 *
 * Flow:
 * 1. Check user in room
 * 2. Remove from participants
 * 3. Update client state
 * 4. Response: 123 ROOM_LEAVE_OK
 */
void handle_leave_room(Server *server, ClientSession *client, Message *msg);

#endif // ROOM_H