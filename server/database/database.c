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

int db_verify_session(Database *db, const char *session_id, char *username_out)
{
    pthread_mutex_lock(&db->mutex);

    char query[512];
    snprintf(query, sizeof(query), "SELECT username FROM sessions WHERE session_id='%s' AND is_active = 1"
                                   "AND TIMESTAMPDIFF(MINUTE, last_activity, NOW()) < 30",
             session_id);

    if (mysql_query(db->conn, query) != 0)
    {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }

    MYSQL_RES *result = mysql_store_result(db->conn);
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row)
    {
        strncpy(username_out, row[0], 255);
        username_out[255] = '\0';
        mysql_free_result(result);
        pthread_mutex_unlock(&db->mutex);
        return 1;
    }

    mysql_free_result(result);
    pthread_mutex_unlock(&db->mutex);
    return 0;
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

int db_cleanup_expired_sessions(Database *db, int timeout_minutes)
{
    pthread_mutex_lock(&db->mutex);

    char query[512];
    snprintf(query, sizeof(query), "UPDATE sessions SET is_active = 0 WHERE TIMESTAMPDIFF(MINUTE, last_activity, NOW()) >= %d", timeout_minutes);
    int result = mysql_query(db->conn, query);

    pthread_mutex_unlock(&db->mutex);
    return result == 0 ? 0 : -1;
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
    char *json = malloc(16384);
    strcpy(json, "{\n  \"rooms\": [\n");

    MYSQL_ROW row;
    int first = 1;
    while ((row = mysql_fetch_row(result)))
    {
        if (!first)
            strcat(json, ",\n");
        first = 0;

        char room_entry[512];
        // row[0]=room_id, row[1]=room_name, row[2]=creator, row[3]=status,
        // row[4]=participant_count, row[5]=max_participants, row[6]=num_questions,
        // row[7]=time_limit_minutes, row[8]=created_at
        snprintf(room_entry, sizeof(room_entry),
                 "{\"room_id\":\"%s\",\"room_name\":\"%s\",\"creator\":\"%s\","
                 "\"status\":\"%s\",\"participant_count\":%s,\"max_participants\":%s,"
                 "\"num_questions\":%s,\"time_limit_minutes\":%s,\"created_at\":\"%s\"}",
                 row[0], row[1], row[2], row[3], row[4], row[5], row[6], row[7], row[8]);
        strcat(json, "    "); // Indent 4 spaces
        strcat(json, room_entry);
        strcat(json, "\n");
    }

    strcat(json, "\n  ]\n}");

    mysql_free_result(result);
    pthread_mutex_unlock(&db->mutex);

    return json;
}
int db_get_room_status(Database *db, const char *room_id)
{
    pthread_mutex_lock(&db->mutex);

    char query[256];
    snprintf(query, sizeof(query),
             "SELECT status FROM rooms WHERE room_id='%s'", room_id);

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