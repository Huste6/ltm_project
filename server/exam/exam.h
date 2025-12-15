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

#endif // EXAM_H