#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>
#include "protocol/protocol.h"
#include "database/database.h"
#include "logger/logger.h"
#include "exam/exam.h"

#define MAX_CLIENTS 100
#define SERVER_PORT 8888
#define SESSION_TIMEOUT_MINUTES 120
#define MAX_QUESTIONS 50

/**
 * @brief Client states
 */
typedef enum
{
    STATE_DISCONNECTED,  // 0 - chưa kết nối
    STATE_CONNECTED,     // 1 - đã kết nối nhưng chưa login
    STATE_AUTHENTICATED, // 2 - đã login thành công
    STATE_IN_PRACTICE,   // 3 - đang luyện tập
    STATE_IN_ROOM,       // 4 - trong phòng chờ
    STATE_IN_EXAM        // 5 - trong phòng thi
} ClientState;

/**
 * @brief Client session structure
 */
typedef struct ClientSession
{
    int socket_fd;
    char session_id[MAX_SESSION_ID_LEN];
    char username[MAX_USERNAME_LEN + 1];
    char current_room[MAX_ROOM_ID_LEN];
    ClientState state;
    time_t last_activity;
    pthread_t thread_id;
    int active;

    // Auto-submit on timeout support
    // Map: question_id -> answer (A/B/C/D or 0 if unanswered)
    // Key is question_id (from DB), value is answer
    int question_ids[MAX_QUESTIONS];  // Store actual DB question IDs
    char exam_answers[MAX_QUESTIONS]; // Answers indexed by position in question_ids array
    int exam_total_questions;
    int has_submitted;
} ClientSession;

typedef struct Server
{
    int server_fd;
    Database *db; // Pointer to database (standard design)
    ClientSession clients[MAX_CLIENTS];
    pthread_mutex_t clients_mutex;
    int running;
    pthread_t cleanup_thread_id; // Thread for timeout auto-finish
} Server;

// Server lifecycle
int server_init(Server *server, int port);
void server_start(Server *server);
void server_stop(Server *server);
void server_cleanup(Server *server);

// Client management
void *handle_client(void *arg);
ClientSession *find_session_by_socket(Server *server, int socket_fd);
ClientSession *find_session_by_username(Server *server, const char *username);
void remove_client_session(Server *server, int socket_fd);

// Utility functions
void send_error_or_response(int socket_fd, int code, const char *message);

#endif // SERVER_H