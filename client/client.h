#ifndef CLIENT_H
#define CLIENT_H

#include "protocol/protocol.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8888
#define BUFFER_SIZE 8192

/**
 * @brief Client states
 */
typedef enum {
    CLIENT_DISCONNECTED,
    CLIENT_CONNECTED,
    CLIENT_AUTHENTICATED,
    CLIENT_IN_ROOM,
    CLIENT_IN_EXAM
} ClientState;

/**
 * @brief Client structure
 */
typedef struct {
    int socket_fd;
    ClientState state;
    char username[MAX_USERNAME_LEN + 1];
    char session_id[MAX_SESSION_ID_LEN];
    char current_room[MAX_ROOM_ID_LEN];
} Client;

/**
 * @brief Initialize and connect to server
 * @param client Client structure
 * @param host Server IP address
 * @param port Server port
 * @return 0 on success, -1 on error
 */
int client_connect(Client *client, const char *host, int port);

/**
 * @brief Disconnect from server
 * @param client Client structure
 */
void client_disconnect(Client *client);

/**
 * @brief Send command to server
 * @param client Client structure
 * @param command Command name
 * @param params Array of parameters
 * @param param_count Number of parameters
 * @return 0 on success, -1 on error
 */
int client_send_command(Client *client, const char *command, const char **params, int param_count);

/**
 * @brief Receive response from server
 * @param client Client structure
 * @param response Response structure to fill
 * @return 0 on success, -1 on error
 */
int client_receive_response(Client *client, Response *response);

#endif // CLIENT_H