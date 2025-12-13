#ifndef DATABASE_H
#define DATABASE_H

#include <mysql/mysql.h>
#include <pthread.h>

typedef struct
{
    MYSQL *conn;
    pthread_mutex_t mutex;
} Database;

// connect to the database
int db_connect(Database *db, const char *host, const char *user, const char *password, const char *dbname);
// connect to the database with specific port
int db_connect_with_port(Database *db, const char *host, const char *user, const char *password, const char *dbname, unsigned int port);
// disconnect from the database
void db_disconnect(Database *db);

// User operations
int db_create_user(Database *db, const char *username, const char *password_hash);
int db_check_username_exists(Database *db, const char *username);
int db_verify_login(Database *db, const char *username, const char *password_hash);
int db_is_account_locked(Database *db, const char *username);

// Session operations
int db_create_session(Database *db, const char *session_id, const char *username);
int db_verify_session(Database *db, const char *session_id, char *username_out);
int db_destroy_session(Database *db, const char *session_id);
int db_check_user_logged_in(Database *db, const char *username);
int db_cleanup_expired_sessions(Database *db, int timeout_minutes);

// Logging
void db_log_activity(Database *db, const char *level, const char *username, const char *action, const char *details);

// Room operations
/**
 * @brief List rooms with optional filter
 * @param db Database structure
 * @param status_filter Status filter (ALL, NOT_STARTED, IN_PROGRESS, FINISHED)
 * @return JSON string (must be freed by caller), NULL on error
 */
char *db_list_rooms(Database *db, const char *status_filter);

/**
 * @brief Get room status
 * @param db Database structure
 * @param room_id Room ID
 * @return 0=NOT_STARTED, 1=IN_PROGRESS, 2=FINISHED, -1=NOT_FOUND
 */
int db_get_room_status(Database *db, const char *room_id);

#endif // DATABASE_H