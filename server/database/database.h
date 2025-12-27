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
// int db_verify_session(Database *db, const char *session_id, char *username_out);
int db_destroy_session(Database *db, const char *session_id);
int db_check_user_logged_in(Database *db, const char *username);
// int db_cleanup_expired_sessions(Database *db, int timeout_minutes);

// Logging
void db_log_activity(Database *db, const char *level, const char *username, const char *action, const char *details);

// Room operations
int db_create_room(Database *db, const char *room_id, const char *room_name, const char *creator, int num_questions, int time_limit);
char *db_list_rooms(Database *db, const char *status_filter);
int db_join_room(Database *db, const char *room_id, const char *username);
int db_get_room_status(Database *db, const char *room_id);
int db_get_room_participant_count(Database *db, const char *room_id);
time_t db_get_room_start_time(Database *db, const char *room_id);

// Exam operations
char *db_get_room_leaderboard(Database *db, const char *room_id);
char *db_get_exam_questions(Database *db, const char *room_id);
int db_leave_room(Database *db, const char *room_id, const char *username);
int db_start_room(Database *db, const char *room_id);
int db_finish_room(Database *db, const char *room_id);
int db_check_and_finish_timed_out_rooms(Database *db);
int db_is_room_expired(Database *db, const char *room_id);
int db_is_room_creator(Database *db, const char *room_id, const char *username);
int db_is_participant(Database *db, const char *room_id, const char *username);
int db_delete_room(Database *db, const char *room_id);

// Submit exam operations
int db_submit_exam(Database *db, const char *room_id, const char *username, int score, int total, const char *answers, int time_taken);
int db_check_already_submitted(Database *db, const char *room_id, const char *username);
char *db_get_exam_result(Database *db, const char *room_id, const char *username);
int db_get_correct_answers(Database *db, const char *room_id, char *answers_out, int *total_out);
int db_check_all_submitted(Database *db, const char *room_id);

#endif // DATABASE_H