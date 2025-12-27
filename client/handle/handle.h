#ifndef HANDLE_H
#define HANDLE_H

#include "../client.h"

/**
 * @brief Handle user registration
 * @param client Client instance
 *
 * Flow:
 * 1. Get username and password from user
 * 2. Send REGISTER command to server
 * 3. Receive and display response
 */
void handle_register(Client *client);

/**
 * @brief Handle user login
 * @param client Client instance
 *
 * Flow:
 * 1. Get username and password from user
 * 2. Send LOGIN command to server
 * 3. Receive response
 * 4. Update client state if successful
 */
void handle_login(Client *client);

/**
 * @brief Handle user logout
 * @param client Client instance
 *
 * Flow:
 * 1. Send LOGOUT command to server
 * 2. Clear client session data
 * 3. Update state to CONNECTED
 */
void handle_logout(Client *client);

/**
 * @brief Handle list rooms
 * @param client Client instance
 *
 * Flow:
 * 1. Get filter parameter (ALL, NOT_STARTED, etc.)
 * 2. Send LIST_ROOMS command
 * 3. Receive and display room list (JSON)
 */
void handle_list_rooms(Client *client);

/**
 * @brief Handle create room
 * @param client Client instance
 *
 * Flow:
 * 1. Get room details from user
 * 2. Send CREATE_ROOM command
 * 3. Receive and display response
 */
void handle_create_room(Client *client);

/**
 * @brief Handle join room
 * @param client Client instance
 *
 * Flow:
 * 1. Get room ID from user
 * 2. Send JOIN_ROOM command
 * 3. Receive and display response
 */
void handle_join_room(Client *client);

/**
 * @brief Handle view result
 * @param client Client instance
 *
 * Flow:
 * 1. Get room_id from user
 * 2. Send VIEW_RESULT command
 * 3. Receive and display leaderboard (JSON)
 */
void handle_view_result(Client *client);

/**
 * @brief Handle leave room
 * @param client Client instance
 *
 * Flow:
 * 1. Send LEAVE_ROOM command
 * 2. Update client state to AUTHENTICATED
 */
void handle_leave_room(Client *client);

/**
 * @brief Handle start exam (creator only)
 * @param client Client instance
 *
 * Flow:
 * 1. Send START_EXAM command
 * 2. Wait for broadcast
 * 3. Update state to IN_EXAM
 */
void handle_start_exam(Client *client);

/**
 * @brief Handle GET_EXAM - fetch exam questions
 * @param client Client instance
 *
 * Flow:
 * 1. Send GET_EXAM command
 * 2. Receive 150 DATA response with JSON
 * 3. Display exam questions
 */
void handle_get_exam(Client *client);

/**
 * @brief Handle SUBMIT_EXAM - submit exam answers
 * @param client Client instance
 *
 * Flow:
 * 1. Get answers from user (comma-separated)
 * 2. Send SUBMIT_EXAM room_id|answers
 * 3. Receive 130 SUBMIT_OK with score|total
 * 4. Display result and percentage
 * 5. Update client state to AUTHENTICATED
 */
void handle_submit_exam(Client *client);

/**
 * @brief Handle PRACTICE - start practice mode
 * @param client Client instance
 *
 * Flow:
 * 1. Get num_questions and time_limit from user
 * 2. Send PRACTICE command to server
 * 3. Receive 140 DATA with questions JSON
 * 4. Display questions and collect answers
 * 5. Submit answers automatically
 */
void handle_practice(Client *client);

/**
 * @brief Handle SUBMIT_PRACTICE - submit practice answers
 * @param client Client instance
 * @param answers Comma-separated answers
 *
 * Flow:
 * 1. Send SUBMIT_PRACTICE practice_id|answers
 * 2. Receive 141 PRACTICE_RESULT score|total
 * 3. Display result
 * 4. Update client state to AUTHENTICATED
 */
void handle_submit_practice(Client *client, const char *answers);

/**
 * @brief Display exam questions from JSON data
 */
void display_exam_questions(const char *json_data, int *question_ids, int total_questions);

/**
 * @brief Answer exam questions - auto-save each answer
 */
void handle_answer_questions(Client *client, const int *question_ids, int total_questions);

/**
 * @brief Display exam result from score|total format
 */
void display_exam_result(const char *result_str);

/**
 * @brief Allow user to modify a saved answer
 */
void modify_answer(Client *client, int *question_ids, int total_questions);

#endif // HANDLE_H