#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int db_connect(Database *db, const char *host, const char *user, const char *password, const char *dbname)
{
    return db_connect_with_port(db, host, user, password, dbname, 3306);
}

int db_connect_with_port(Database *db, const char *host, const char *user, const char *password, const char *dbname, unsigned int port)
{
    db->conn = mysql_init(NULL);
    if (db->conn == NULL)
    {
        fprintf(stderr, "mysql_init() failed\n");
        return -1;
    }

    // Kết nối với port cụ thể
    if (mysql_real_connect(db->conn, host, user, password, dbname, port, NULL, 0) == NULL)
    {
        fprintf(stderr, "mysql_real_connect() failed: %s\n", mysql_error(db->conn));
        mysql_close(db->conn);
        return -1;
    }

    if (pthread_mutex_init(&db->mutex, NULL) != 0)
    {
        fprintf(stderr, "Mutex init failed\n");
        mysql_close(db->conn);
        return -1;
    }

    return 0;
}

void db_disconnect(Database *db)
{
    pthread_mutex_destroy(&db->mutex);
    mysql_close(db->conn);
}

// ============================= User operations ===============================
int db_create_user(Database *db, const char *username, const char *password_hash)
{
    pthread_mutex_lock(&db->mutex);

    char query[512];
    snprintf(query, sizeof(query), "INSERT INTO users (username, password_hash) VALUES ('%s', '%s')", username, password_hash);

    int result = mysql_query(db->conn, query);
    pthread_mutex_unlock(&db->mutex);
    return result == 0 ? 0 : -1;
}

int db_check_username_exists(Database *db, const char *username)
{
    pthread_mutex_lock(&db->mutex);

    char query[256];
    snprintf(query, sizeof(query), "SELECT COUNT(*) FROM users WHERE username='%s'", username);

    // conn dùng để thực hiện truy vấn MySQL
    if (mysql_query(db->conn, query) != 0)
    {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    MYSQL_RES *result = mysql_store_result(db->conn); // Lấy kết quả truy vấn
    MYSQL_ROW row = mysql_fetch_row(result);          // Lấy dòng đầu tiên
    int exists = atoi(row[0]) > 0;                    // Nếu > 0 thì tồn tại
    mysql_free_result(result);
    pthread_mutex_unlock(&db->mutex);
    return exists;
}

int db_verify_login(Database *db, const char *username, const char *password_hash)
{
    pthread_mutex_lock(&db->mutex);

    char query[512];
    snprintf(query, sizeof(query), "SELECT COUNT(*) FROM users WHERE username='%s' AND password_hash='%s'", username, password_hash);
    if (mysql_query(db->conn, query) != 0)
    {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    MYSQL_RES *result = mysql_store_result(db->conn);
    MYSQL_ROW row = mysql_fetch_row(result);
    int valid = atoi(row[0]) > 0;

    mysql_free_result(result);
    pthread_mutex_unlock(&db->mutex);

    return valid;
}

int db_is_account_locked(Database *db, const char *username)
{
    pthread_mutex_lock(&db->mutex);

    char query[256];
    snprintf(query, sizeof(query), "SELECT is_locked FROM users WHERE username='%s'", username);
    if (mysql_query(db->conn, query) != 0)
    {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }

    MYSQL_RES *result = mysql_store_result(db->conn);
    MYSQL_ROW row = mysql_fetch_row(result);
    int is_locked = 0;
    if (row != NULL)
    {
        is_locked = atoi(row[0]);
    }

    mysql_free_result(result);
    pthread_mutex_unlock(&db->mutex);

    return is_locked;
}

// ============================ Session operations =============================
int db_create_session(Database *db, const char *session_id, const char *username)
{
    pthread_mutex_lock(&db->mutex);

    // Deactivate existing sessions
    char query1[512];
    snprintf(query1, sizeof(query1), "UPDATE sessions SET is_active = 0 WHERE username='%s'", username);
    mysql_query(db->conn, query1);

    // Create new session
    char query2[512];
    snprintf(query2, sizeof(query2), "INSERT INTO sessions (session_id, username) VALUES ('%s', '%s')", session_id, username);
    int result = mysql_query(db->conn, query2);

    pthread_mutex_unlock(&db->mutex);
    return result == 0 ? 0 : -1;
}

int db_destroy_session(Database *db, const char *session_id)
{
    pthread_mutex_lock(&db->mutex);

    char query[512];
    snprintf(query, sizeof(query), "UPDATE sessions SET is_active = 0 WHERE session_id='%s'", session_id);
    int result = mysql_query(db->conn, query);

    pthread_mutex_unlock(&db->mutex);
    return result == 0 ? 0 : -1;
}

int db_check_user_logged_in(Database *db, const char *username)
{
    pthread_mutex_lock(&db->mutex);

    char query[512];
    snprintf(query, sizeof(query), "SELECT COUNT(*) FROM sessions WHERE username='%s' AND is_active = 1", username);
    if (mysql_query(db->conn, query) != 0)
    {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }

    MYSQL_RES *result = mysql_store_result(db->conn);
    MYSQL_ROW row = mysql_fetch_row(result);
    int logged_in = atoi(row[0]) > 0;

    mysql_free_result(result);
    pthread_mutex_unlock(&db->mutex);

    return logged_in;
}

// =============================== Logging =====================================
void db_log_activity(Database *db, const char *level, const char *username, const char *action, const char *details)
{
    pthread_mutex_lock(&db->mutex);

    char query[2048];
    char safe_details[1024] = {0};

    if (details)
    {
        mysql_real_escape_string(db->conn, safe_details, details, strlen(details));
    }

    snprintf(query, sizeof(query),
             "INSERT INTO activity_logs (level, username, action, details) "
             "VALUES ('%s', '%s', '%s', '%s')",
             level, username ? username : "SYSTEM", action, safe_details);

    if (mysql_query(db->conn, query) != 0)
    {
        fprintf(stderr, "Failed to log activity: %s\n", mysql_error(db->conn));
    }
    pthread_mutex_unlock(&db->mutex);
}

// ============================= Room operations ===============================
int db_create_room(Database *db, const char *room_id, const char *room_name, const char *creator, int num_questions, int time_limit)
{
    pthread_mutex_lock(&db->mutex);

    // Start transaction
    if (mysql_query(db->conn, "START TRANSACTION"))
    {
        fprintf(stderr, "[DB ERROR] Failed to start transaction: %s\n", mysql_error(db->conn));
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }

    // Insert room (max_participants will use default value from schema)
    char query1[512];
    snprintf(query1, sizeof(query1),
             "INSERT INTO rooms (room_id, room_name, creator, num_questions, time_limit_minutes) "
             "VALUES ('%s', '%s', '%s', %d, %d)",
             room_id, room_name, creator, num_questions, time_limit);

    if (mysql_query(db->conn, query1))
    {
        fprintf(stderr, "[DB ERROR] Failed to create room: %s\n", mysql_error(db->conn));
        mysql_query(db->conn, "ROLLBACK");
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }

    // Select random questions and insert into room_questions
    char query2[1024];
    snprintf(query2, sizeof(query2),
             "INSERT INTO room_questions (room_id, question_id, question_order) "
             "SELECT '%s', id, (@row_number := @row_number + 1) "
             "FROM questions, (SELECT @row_number := 0) AS t "
             "ORDER BY RAND() LIMIT %d",
             room_id, num_questions);

    if (mysql_query(db->conn, query2))
    {
        fprintf(stderr, "[DB ERROR] Failed to assign questions: %s\n", mysql_error(db->conn));
        mysql_query(db->conn, "ROLLBACK");
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }

    // Creator auto joins, add user to participants table
    char query3[512];
    snprintf(query3, sizeof(query3),
             "INSERT INTO participants (room_id, username) VALUES ('%s', '%s')",
             room_id, creator);

    if (mysql_query(db->conn, query3))
    {
        fprintf(stderr, "[DB ERROR] Failed to add creator as participant: %s\n", mysql_error(db->conn));
        mysql_query(db->conn, "ROLLBACK");
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }

    // Commit transaction
    if (mysql_query(db->conn, "COMMIT"))
    {
        fprintf(stderr, "[DB ERROR] Failed to commit transaction: %s\n", mysql_error(db->conn));
        mysql_query(db->conn, "ROLLBACK");
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }

    pthread_mutex_unlock(&db->mutex);

    printf("[DB] Room '%s' created with %d random questions assigned\n", room_id, num_questions);
    return 0;
}

char *db_list_rooms(Database *db, const char *status_filter)
{
    pthread_mutex_lock(&db->mutex);

    char query[1024];
    if (strcmp(status_filter, "ALL") == 0)
    {
        snprintf(query, sizeof(query),
                 "SELECT r.room_id, r.room_name, r.creator, r.status, "
                 "COALESCE(COUNT(p.username), 0) as participant_count, "
                 "r.max_participants, r.num_questions, r.time_limit_minutes, r.created_at "
                 "FROM rooms r LEFT JOIN participants p ON r.room_id = p.room_id "
                 "GROUP BY r.room_id ORDER BY r.created_at DESC");
    }
    else
    {
        snprintf(query, sizeof(query),
                 "SELECT r.room_id, r.room_name, r.creator, r.status, "
                 "COALESCE(COUNT(p.username), 0) as participant_count, "
                 "r.max_participants, r.num_questions, r.time_limit_minutes, r.created_at "
                 "FROM rooms r LEFT JOIN participants p ON r.room_id = p.room_id "
                 "WHERE r.status='%s' "
                 "GROUP BY r.room_id ORDER BY r.created_at DESC",
                 status_filter);
    }

    if (mysql_query(db->conn, query)) // Execute query, if error(mysql_query return non-zero), return NULL
    {
        pthread_mutex_unlock(&db->mutex);
        return NULL;
    }

    // Retrieve results, mysql_store_result fetches the result set from the last query
    MYSQL_RES *result = mysql_store_result(db->conn);
    if (!result)
    {
        pthread_mutex_unlock(&db->mutex);
        return NULL;
    }

    // Build JSON
    char *json = malloc(16384);
    strcpy(json, "{\n  \"rooms\": [\n");

    MYSQL_ROW row; // Fetch each row from the result set
    while ((row = mysql_fetch_row(result)))
    {
        char room_entry[512]; // Temporary buffer for each room entry
        // row[0]=room_id, row[1]=room_name, row[2]=creator, row[3]=status,
        // row[4]=participant_count, row[5]=max_participants, row[6]=num_questions,
        // row[7]=time_limit_minutes, row[8]=created_at
        snprintf(room_entry, sizeof(room_entry),
                 "{\"room_id\":\"%s\",\"room_name\":\"%s\",\"creator\":\"%s\","
                 "\"status\":\"%s\",\"participant_count\":%s,\"max_participants\":%s,"
                 "\"num_questions\":%s,\"time_limit_minutes\":%s,\"created_at\":\"%s\"},",
                 row[0], row[1], row[2], row[3], row[4], row[5], row[6], row[7], row[8]);
        strcat(json, room_entry); // Append room entry to JSON
        strcat(json, "\n");
    }

    strcat(json, "  ]\n}");
    // json:
    // {
    //   "rooms": [
    //     {
    //       "room_id": "1234567890",
    //       "room_name": "Sample Room",
    //       "creator": "user1",
    //       "status": "NOT_STARTED",
    //       "participant_count": 5,
    //       "max_participants": 10,
    //       "num_questions": 20,
    //       "time_limit_minutes": 15,
    //       "created_at": "2024-10-01 12:34:56"
    //     },
    //     ...
    //   ]
    // }

    mysql_free_result(result); // Free the result set to avoid memory leaks
    pthread_mutex_unlock(&db->mutex);

    return json;
}

/**
 * @brief Add user to room participants
 * @param db Pointer to Database
 * @param room_id Room ID
 * @param username Username to add
 * @return 0 on success, -1 on failure
 */
int db_join_room(Database *db, const char *room_id, const char *username)
{
    pthread_mutex_lock(&db->mutex);

    char query[512];
    snprintf(query, sizeof(query),
             "INSERT IGNORE INTO participants (room_id, username) VALUES ('%s', '%s')", room_id, username);
    // Use INSERT IGNORE to avoid duplicate entries

    int result = mysql_query(db->conn, query);

    pthread_mutex_unlock(&db->mutex);
    return result == 0 ? 0 : -1; // Return 0 on success, -1 on failure
}

/**
 * @brief Get room status
 * @param db Pointer to Database
 * @param room_id Room ID
 * @return Room status as integer (0=NOT_STARTED, 1=IN_PROGRESS, 2=FINISHED), -1 on error
 */
int db_get_room_status(Database *db, const char *room_id)
{
    pthread_mutex_lock(&db->mutex);

    char query[256];
    snprintf(query, sizeof(query), "SELECT status FROM rooms WHERE room_id='%s'", room_id);

    if (mysql_query(db->conn, query))
    {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }

    MYSQL_RES *result = mysql_store_result(db->conn);
    if (!result)
    {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    int status = -1;

    if (row)
    {
        if (strcmp(row[0], "NOT_STARTED") == 0)
            status = 0;
        else if (strcmp(row[0], "IN_PROGRESS") == 0)
            status = 1;
        else if (strcmp(row[0], "FINISHED") == 0)
            status = 2;
    }

    mysql_free_result(result);
    pthread_mutex_unlock(&db->mutex);

    return status;
}

/**
 * @brief Get number of participants in a room
 * @param db Pointer to Database
 * @param room_id Room ID
 * @return Number of participants, -1 on error
 */
int db_get_room_participant_count(Database *db, const char *room_id)
{
    pthread_mutex_lock(&db->mutex);

    char query[256];
    snprintf(query, sizeof(query),
             "SELECT COUNT(*) FROM participants WHERE room_id='%s'", room_id);

    if (mysql_query(db->conn, query))
    {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }

    MYSQL_RES *result = mysql_store_result(db->conn);
    if (!result)
    {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }

    MYSQL_ROW row = mysql_fetch_row(result);

    // If row is NULL, return 0 participants
    int count = row ? atoi(row[0]) : 0;

    mysql_free_result(result);
    pthread_mutex_unlock(&db->mutex);

    return count;
}

/**
 * @brief Get leaderboard for room
 * @param db Database structure
 * @param room_id Room ID
 * @return JSON string with leaderboard (must be freed), NULL on error
 */
char *db_get_room_leaderboard(Database *db, const char *room_id)
{
    pthread_mutex_lock(&db->mutex);

    char query[1024];
    snprintf(query, sizeof(query),
             "SELECT username, score, total_questions, submit_time, time_taken_seconds "
             "FROM exam_results WHERE room_id='%s' "
             "ORDER BY score DESC, submit_time ASC",
             room_id);

    if (mysql_query(db->conn, query))
    {
        pthread_mutex_unlock(&db->mutex);
        return NULL;
    }

    MYSQL_RES *result = mysql_store_result(db->conn);
    if (!result)
    {
        pthread_mutex_unlock(&db->mutex);
        return NULL;
    }

    // Build JSON
    // json: {"leaderboard":[{"rank":1,"username":"user1","score":8,"total":10,"submit_time":"2024-10-01 12:00:00", "time_taken":120},...]}
    char *json = malloc(16384);
    strcpy(json, "{\n  \"leaderboard\":[\n");

    MYSQL_ROW row;
    int rank = 1;
    while ((row = mysql_fetch_row(result)))
    {
        char entry[512];
        snprintf(entry, sizeof(entry),
                 "{\"rank\":%d,\"username\":\"%s\",\"score\":%s,"
                 "\"total\":%s,\"submit_time\":\"%s\",\"time_taken\":%s},",
                 rank++, row[0], row[1], row[2], row[3], row[4]);
        strcat(json, "    ");
        strcat(json, entry);
        strcat(json, "\n");
    }

    strcat(json, "]}");

    mysql_free_result(result);
    pthread_mutex_unlock(&db->mutex);

    return json;
}

/**
 * @brief Get exam questions for a room (without correct answers for security)
 * @param db Pointer to Database
 * @param room_id Room ID
 * @return JSON string with questions array, NULL on error
 *
 * Format: {"questions":[{"question_id":1,"content":"...","options":["A","B","C","D"]},...]}}
 * IMPORTANT: NEVER include "correct_answer" field!
 */
char *db_get_exam_questions(Database *db, const char *room_id)
{
    pthread_mutex_lock(&db->mutex);

    // Query to get questions for this room
    // Join room_questions with questions table to get full question data
    char query[2048];
    snprintf(query, sizeof(query),
             "SELECT q.id, q.question_text, q.option_a, q.option_b, q.option_c, q.option_d "
             "FROM room_questions rq "
             "JOIN questions q ON rq.question_id = q.id "
             "WHERE rq.room_id='%s' "
             "ORDER BY rq.question_order ASC",
             room_id);

    if (mysql_query(db->conn, query))
    {
        fprintf(stderr, "db_get_exam_questions query failed: %s\n", mysql_error(db->conn));
        pthread_mutex_unlock(&db->mutex);
        return NULL;
    }

    MYSQL_RES *result = mysql_store_result(db->conn);
    if (!result)
    {
        fprintf(stderr, "db_get_exam_questions mysql_store_result failed\n");
        pthread_mutex_unlock(&db->mutex);
        return NULL;
    }

    // Build JSON response with questions
    char *json = malloc(1024 * 1024); // 1MB buffer for exam data
    if (!json)
    {
        mysql_free_result(result);
        pthread_mutex_unlock(&db->mutex);
        return NULL;
    }

    strcpy(json, "{\n  \"questions\": [\n");

    MYSQL_ROW row;
    int first = 1;
    while ((row = mysql_fetch_row(result)))
    {
        if (!first)
            strcat(json, ",\n");
        first = 0;

        // Escape special characters in options for JSON
        char opt_a[256], opt_b[256], opt_c[256], opt_d[256];
        snprintf(opt_a, sizeof(opt_a), "A. %s", row[2]);
        snprintf(opt_b, sizeof(opt_b), "B. %s", row[3]);
        snprintf(opt_c, sizeof(opt_c), "C. %s", row[4]);
        snprintf(opt_d, sizeof(opt_d), "D. %s", row[5]);

        // Build question entry with proper formatting
        char entry[4096];
        snprintf(entry, sizeof(entry),
                 "    {\n"
                 "      \"question_id\": %s,\n"
                 "      \"content\": \"%s\",\n"
                 "      \"options\": [\n"
                 "        \"%s\",\n"
                 "        \"%s\",\n"
                 "        \"%s\",\n"
                 "        \"%s\"\n"
                 "      ]\n"
                 "    }",
                 row[0], // question_id
                 row[1], // question_text (content)
                 opt_a, opt_b, opt_c, opt_d);

        strcat(json, entry);
    }

    strcat(json, "\n  ]\n}");

    mysql_free_result(result);
    pthread_mutex_unlock(&db->mutex);

    return json;
}

/**
 * @brief Remove user from room participants
 * @param db Pointer to Database
 * @param room_id Room ID
 * @param username Username to remove
 * @return 0 on success, -1 on failure
 */
int db_leave_room(Database *db, const char *room_id, const char *username)
{
    pthread_mutex_lock(&db->mutex);

    char query[512];
    snprintf(query, sizeof(query), "DELETE FROM participants WHERE room_id='%s' AND username='%s'", room_id, username);

    int result = mysql_query(db->conn, query);

    pthread_mutex_unlock(&db->mutex);
    return result == 0 ? 0 : -1;
}

/**
 * @brief Start a room exam (change status from NOT_STARTED to IN_PROGRESS)
 * @param db Pointer to Database
 * @param room_id Room ID
 * @return 0 on success, -1 on failure
 */
int db_start_room(Database *db, const char *room_id)
{
    pthread_mutex_lock(&db->mutex);

    char query[512];

    // Update room status to IN_PROGRESS and set start_time to NOW()
    snprintf(query, sizeof(query), "UPDATE rooms SET status='IN_PROGRESS', start_time=NOW() WHERE room_id='%s'", room_id);

    int result = mysql_query(db->conn, query);

    pthread_mutex_unlock(&db->mutex);
    return result == 0 ? 0 : -1;
}

/**
 * @brief Finish a room exam (change status to FINISHED)
 * @param db Pointer to Database
 * @param room_id Room ID
 * @return 0 on success, -1 on failure
 */
int db_finish_room(Database *db, const char *room_id)
{
    pthread_mutex_lock(&db->mutex);

    char query[512];
    snprintf(query, sizeof(query),
             "UPDATE rooms SET status='FINISHED', finish_time=NOW() WHERE room_id='%s'",
             room_id);

    int result = mysql_query(db->conn, query);

    pthread_mutex_unlock(&db->mutex);

    if (result == 0)
    {
        printf("[DB] Room '%s' marked as FINISHED\n", room_id);
    }

    return result == 0 ? 0 : -1;
}

/**
 * @brief Check if user is the creator of a room
 * @param db Pointer to Database
 * @param room_id Room ID
 * @param username Username to check
 * @return 1 if user is creator, 0 otherwise
 */
int db_is_room_creator(Database *db, const char *room_id, const char *username)
{
    pthread_mutex_lock(&db->mutex);

    char query[256];
    snprintf(query, sizeof(query), "SELECT COUNT(*) FROM rooms WHERE room_id='%s' AND creator='%s'", room_id, username);

    if (mysql_query(db->conn, query))
    {
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }

    MYSQL_RES *result = mysql_store_result(db->conn);
    if (!result)
    {
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    int is_creator = row ? (atoi(row[0]) > 0) : 0; // If count > 0, user is creator

    mysql_free_result(result);
    pthread_mutex_unlock(&db->mutex);

    return is_creator;
}

/**
 * @brief Check if user is a participant in room
 * @param db Pointer to Database
 * @param room_id Room ID
 * @param username Username to check
 * @return 1 if user is participant, 0 otherwise
 */
int db_is_participant(Database *db, const char *room_id, const char *username)
{
    pthread_mutex_lock(&db->mutex);

    char query[256];
    snprintf(query, sizeof(query), "SELECT COUNT(*) FROM participants WHERE room_id='%s' AND username='%s'", room_id, username);

    if (mysql_query(db->conn, query))
    {
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }

    MYSQL_RES *result = mysql_store_result(db->conn);
    if (!result)
    {
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }

    MYSQL_ROW row = mysql_fetch_row(result);    // Fetch the first row from result, which contains the count
    int in_room = row ? (atoi(row[0]) > 0) : 0; // If count > 0, user is in room

    mysql_free_result(result);
    pthread_mutex_unlock(&db->mutex);

    return in_room;
}

/**
 * @brief Delete a room and all its related data
 * @param db Pointer to Database
 * @param room_id Room ID to delete
 * @return 0 on success, -1 on error
 *
 * NOTE: This will cascade delete:
 * - All participants in room_participants (ON DELETE CASCADE)
 * - All room_questions (ON DELETE CASCADE)
 * - All exam_results (ON DELETE CASCADE)
 */
int db_delete_room(Database *db, const char *room_id)
{
    pthread_mutex_lock(&db->mutex);

    char query[256];
    snprintf(query, sizeof(query), "DELETE FROM rooms WHERE room_id='%s'", room_id);

    if (mysql_query(db->conn, query))
    {
        fprintf(stderr, "[DB ERROR] Failed to delete room '%s': %s\n", room_id, mysql_error(db->conn));
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }

    int affected = mysql_affected_rows(db->conn); // Check if any row was deleted
    pthread_mutex_unlock(&db->mutex);

    if (affected == 0) // No room found with given ID
    {
        fprintf(stderr, "[DB WARNING] Room '%s' not found for deletion\n", room_id);
        return -1;
    }

    printf("[DB] Room '%s' deleted successfully (cascaded to participants, questions, results)\n", room_id);
    return 0;
}

// ==========================================
// SUBMIT EXAM OPERATIONS
// ==========================================

/**
 * @brief Get correct answers for a room
 * @param db Database pointer
 * @param room_id Room ID
 * @param answers_out Buffer to store answers (e.g., "ABCDABCD...")
 * @param total_out Pointer to store total number of questions
 * @return 0 on success, -1 on error
 * Flow:
 * 1. Query room_questions joined with questions to get correct answers
 * 2. Store answers in answers_out as a string of characters
 * 3. Set total_out to number of questions
 */
int db_get_correct_answers(Database *db, const char *room_id, char *answers_out, int *total_out)
{
    pthread_mutex_lock(&db->mutex);

    char query[1024];
    snprintf(query, sizeof(query),
             "SELECT q.correct_answer FROM room_questions rq "
             "JOIN questions q ON rq.question_id = q.id "
             "WHERE rq.room_id = '%s' ORDER BY rq.question_order",
             room_id);

    if (mysql_query(db->conn, query))
    {
        fprintf(stderr, "[DB ERROR] Failed to get correct answers: %s\n", mysql_error(db->conn));
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }

    MYSQL_RES *result = mysql_store_result(db->conn);
    if (!result)
    {
        fprintf(stderr, "[DB ERROR] Failed to store result: %s\n", mysql_error(db->conn));
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }

    MYSQL_ROW row;
    int count = 0;
    while ((row = mysql_fetch_row(result)))
    {
        answers_out[count++] = row[0][0]; // Get first character (A, B, C, or D)
    }
    answers_out[count] = '\0'; // Null-terminate the string

    *total_out = count; // Set total number of questions

    mysql_free_result(result);
    pthread_mutex_unlock(&db->mutex);

    return 0;
}

/**
 * @brief Submit exam answers and calculate score
 * @param db Database pointer
 * @param room_id Room ID
 * @param username Username
 * @param score Score achieved
 * @param total Total questions
 * @param answers User's answers (comma-separated: A,B,C,D...)
 * @param time_taken Time taken in seconds
 * @return 0 on success, -1 on error
 */
int db_submit_exam(Database *db, const char *room_id, const char *username, int score, int total, const char *answers, int time_taken)
{
    pthread_mutex_lock(&db->mutex);

    // Escape the answers string to prevent SQL injection
    char escaped_answers[4096];
    mysql_real_escape_string(db->conn, escaped_answers, answers, strlen(answers));

    char query[8192];
    snprintf(query, sizeof(query),
             "INSERT INTO exam_results (room_id, username, score, total_questions, "
             "answers, time_taken_seconds) VALUES ('%s', '%s', %d, %d, '%s', %d)",
             room_id, username, score, total, escaped_answers, time_taken);

    if (mysql_query(db->conn, query))
    {
        fprintf(stderr, "[DB ERROR] Failed to submit exam: %s\n", mysql_error(db->conn));
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }

    pthread_mutex_unlock(&db->mutex);
    printf("[DB] Exam submitted for user '%s' in room '%s': %d/%d\n", username, room_id, score, total);

    return 0;
}

/**
 * @brief Check if user already submitted exam for this room
 * @param db Database pointer
 * @param room_id Room ID
 * @param username Username
 * @return 1 if submitted, 0 if not
 */
int db_check_already_submitted(Database *db, const char *room_id, const char *username)
{
    pthread_mutex_lock(&db->mutex);

    char query[512];
    snprintf(query, sizeof(query),
             "SELECT COUNT(*) FROM exam_results WHERE room_id='%s' AND username='%s'",
             room_id, username);

    if (mysql_query(db->conn, query))
    {
        fprintf(stderr, "[DB ERROR] Failed to check submission: %s\n", mysql_error(db->conn));
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }

    MYSQL_RES *result = mysql_store_result(db->conn);
    if (!result)
    {
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    int submitted = row ? (atoi(row[0]) > 0) : 0;

    mysql_free_result(result);
    pthread_mutex_unlock(&db->mutex);

    return submitted;
}

/**
 * @brief Get exam result for user
 * @param db Database pointer
 * @param room_id Room ID
 * @param username Username
 * @return String with score (e.g., "18|20"), must be freed, NULL on error
 */
char *db_get_exam_result(Database *db, const char *room_id, const char *username)
{
    pthread_mutex_lock(&db->mutex);

    char query[512];
    snprintf(query, sizeof(query),
             "SELECT score, total_questions FROM exam_results "
             "WHERE room_id='%s' AND username='%s'",
             room_id, username);

    if (mysql_query(db->conn, query))
    {
        fprintf(stderr, "[DB ERROR] Failed to get result: %s\n", mysql_error(db->conn));
        pthread_mutex_unlock(&db->mutex);
        return NULL;
    }

    MYSQL_RES *result = mysql_store_result(db->conn);
    if (!result)
    {
        pthread_mutex_unlock(&db->mutex);
        return NULL;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    char *result_str = NULL; // Format: "score|total"

    if (row)
    {
        result_str = malloc(64);
        snprintf(result_str, 64, "%s|%s", row[0], row[1]);
    }

    mysql_free_result(result);
    pthread_mutex_unlock(&db->mutex);

    return result_str;
}

/**
 * @brief Check if all participants in a room have submitted their exams
 * @param db Database pointer
 * @param room_id Room ID
 * @return 1 if all submitted, 0 otherwise
 */
int db_check_all_submitted(Database *db, const char *room_id)
{
    pthread_mutex_lock(&db->mutex);

    // Get total participants count
    char query1[512];
    snprintf(query1, sizeof(query1),
             "SELECT COUNT(*) FROM participants WHERE room_id='%s'",
             room_id);

    if (mysql_query(db->conn, query1))
    {
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }

    MYSQL_RES *result1 = mysql_store_result(db->conn);
    if (!result1)
    {
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }

    MYSQL_ROW row1 = mysql_fetch_row(result1);
    int total_participants = row1 ? atoi(row1[0]) : 0;
    mysql_free_result(result1);

    if (total_participants == 0)
    {
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }

    // Get total submissions count
    char query2[512];
    snprintf(query2, sizeof(query2),
             "SELECT COUNT(*) FROM exam_results WHERE room_id='%s'",
             room_id);

    if (mysql_query(db->conn, query2))
    {
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }

    MYSQL_RES *result2 = mysql_store_result(db->conn);
    if (!result2)
    {
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }

    MYSQL_ROW row2 = mysql_fetch_row(result2);
    int total_submissions = row2 ? atoi(row2[0]) : 0; // Get submission count, if NULL return 0
    mysql_free_result(result2);

    pthread_mutex_unlock(&db->mutex);

    // All submitted if counts match
    return (total_submissions >= total_participants); // creator is not in participants
}
