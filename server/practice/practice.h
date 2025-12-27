#ifndef PRACTICE_H
#define PRACTICE_H

#include "../protocol/protocol.h"
#include "../database/database.h"

typedef struct ClientSession ClientSession;
typedef struct Server Server;

/**
 * @brief Xử lý chế độ luyện tập cá nhân
 * @param server Pointer tới Server instance
 * @param client Pointer tới ClientSession
 * @param msg Message đã parse (PRACTICE num_questions|time_limit)
 *
 * Flow:
 * 1. Check authentication
 * 2. Validate params (5-50 questions, 5-120 minutes)
 * 3. Generate practice_id unique
 * 4. Random select questions from DB
 * 5. Create practice session
 * 6. Response: 140 DATA <length>\n<JSON questions>
 * 7. Update client state to IN_PRACTICE
 */
void handle_practice(Server *server, ClientSession *client, Message *msg);

/**
 * @brief Xử lý nộp bài luyện tập
 * @param server Pointer tới Server instance
 * @param client Pointer tới ClientSession
 * @param msg Message đã parse (SUBMIT_PRACTICE practice_id|answers)
 *
 * Flow:
 * 1. Check authentication
 * 2. Validate practice_id exists
 * 3. Grade answers
 * 4. Save score
 * 5. Response: 141 PRACTICE_RESULT <score>|<total>
 * 6. Update client state back to AUTHENTICATED
 */
void handle_submit_practice(Server *server, ClientSession *client, Message *msg);

#endif // PRACTICE_H
