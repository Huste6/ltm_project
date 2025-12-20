#ifndef EXAM_H
#define EXAM_H

#include "../protocol/protocol.h"
#include "../database/database.h"

typedef struct ClientSession ClientSession;
typedef struct Server Server;

/**
 * @brief Xử lý xem kết quả thi
 * @param server Pointer tới Server instance
 * @param client Pointer tới ClientSession
 * @param msg Message đã parse (VIEW_RESULT room_id)
 *
 * Flow:
 * 1. Check room status = FINISHED
 * 2. Check user participated
 * 3. Get leaderboard từ DB (JSON)
 * 4. Response: 127 DATA <length>\n<JSON leaderboard>
 */
void handle_view_result(Server *server, ClientSession *client, Message *msg);

/**
 * @brief Xử lý lấy câu hỏi bài thi cho người dùng
 * @param server Pointer tới Server instance
 * @param client Pointer tới ClientSession
 * @param msg Message đã parse (GET_EXAM room_id)
 *
 * Flow:
 * 1. Check user authentication (221 if not)
 * 2. Check room exists (223 if not)
 * 3. Check room status NOT_STARTED (224 if true)
 * 4. Check room status FINISHED (225 if true)
 * 5. Check user in room (227 if not)
 * 6. Get exam questions from DB (JSON format, NO correct_answer)
 * 7. Response: 150 DATA <length>\n<JSON questions>
 */
void handle_get_exam(Server *server, ClientSession *client, Message *msg);

/**
 * @brief Xử lý bắt đầu bài thi (chỉ creator)
 * @param server Pointer tới Server instance
 * @param client Pointer tới ClientSession
 * @param msg Message đã parse (START_EXAM room_id)
 *
 * Flow:
 * 1. Check user là creator
 * 2. Check room status = NOT_STARTED
 * 3. Update room status = IN_PROGRESS
 * 4. Set start_time
 * 5. BROADCAST message 125 START_OK tới all participants
 * 6. Update tất cả client sessions trong room
 */
void handle_start_exam(Server *server, ClientSession *client, Message *msg);

/**
 * @brief Broadcast message tới tất cả participants trong room
 * @param server Server instance
 * @param room_id Room ID
 * @param message Message cần broadcast
 */
void broadcast_to_room(Server *server, const char *room_id, const char *message);

#endif // EXAM_H