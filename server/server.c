#include "server.h"
#include "auth/auth.h"
#include "room/room.h"
// #include "exam/exam.h"
// #include "practice/practice.h"
#include "logger/logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// global server instance
Server *g_server = NULL;

/**
 * @brief Initialize the server
 */
int server_init(Server *server, int port)
{
    g_server = server;
    memset(server, 0, sizeof(Server));

    // initialize logger
    if (logger_init("server.log") < 0)
    {
        fprintf(stderr, "Failed to initialize logger\n");
        return -1;
    }
    log_event(LOG_INFO, NULL, "SERVER", "Starting server initialization");

    // Initialize database (allocate memory for Database struct)
    server->db = malloc(sizeof(Database));
    if (!server->db)
    {
        fprintf(stderr, "Failed to allocate memory for database\n");
        log_event(LOG_ERROR, NULL, "SERVER", "Database memory allocation failed");
        return -1;
    }

    const char *host = "127.0.0.1";
    const char *user = "exam_user";
    const char *password = "exam123456";
    const char *dbname = "exam_system";
    unsigned int port_db = 3306;

    if (db_connect_with_port(server->db, host, user, password, dbname, port_db) < 0)
    {
        fprintf(stderr, "Failed to initialize database\n");
        log_event(LOG_ERROR, NULL, "SERVER", "Database initialization failed");
        free(server->db);
        server->db = NULL;
        return -1;
    }
    printf("Database connected successfully\n");
    log_event(LOG_INFO, NULL, "SERVER", "Database connected successfully");

    // initialize mutex
    pthread_mutex_init(&server->clients_mutex, NULL);

    // create socket
    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_fd < 0)
    {
        perror("Socket creation failed");
        log_event(LOG_ERROR, NULL, "SERVER", "Socket creation failed");
        return -1;
    }

    // set socket options
    int opt = 1;
    setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // bind socket
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server->server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        log_event(LOG_ERROR, NULL, "SERVER", "Socket bind failed");
        close(server->server_fd);
        return -1;
    }

    // listen for connections
    if (listen(server->server_fd, 10) < 0)
    {
        perror("Listen failed");
        log_event(LOG_ERROR, NULL, "SERVER", "Socket listen failed");
        close(server->server_fd);
        return -1;
    }
    server->running = 1;
    printf("Server initialized and listening on port %d\n", port);
    log_event(LOG_INFO, NULL, "SERVER", "Server initialized and listening on port %d", port);
    return 0;
}

/**
 * @brief Start the server main loop
 */
void server_start(Server *server)
{
    printf("=== EXAM SERVER STARTED ===\n");
    log_event(LOG_INFO, NULL, "SERVER", "Exam server started successfully");

    while (server->running)
    {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(server->server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0)
        {
            if (server->running)
            {
                perror("Accept failed");
                log_event(LOG_ERROR, NULL, "SERVER", "Accept failed");
            }
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

        printf("New connection from %s:%d\n", client_ip, ntohs(client_addr.sin_port));
        log_event(LOG_INFO, NULL, "CONNECTION", "New connection from %s:%d", client_ip, ntohs(client_addr.sin_port));

        // find free slot for new client
        pthread_mutex_lock(&server->clients_mutex);
        int slot = -1;
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (!server->clients[i].active)
            {
                slot = i;
                break;
            }
        }
        if (slot == -1)
        {
            pthread_mutex_unlock(&server->clients_mutex);
            send_error_or_response(client_fd, CODE_INTERNAL_ERROR, "Server full");
            close(client_fd);
            log_event(LOG_WARNING, NULL, "CONNECTION", "Connection from %s:%d rejected: server full", client_ip, ntohs(client_addr.sin_port));
            continue;
        }

        // initialize client session
        ClientSession *client = &server->clients[slot];
        memset(client, 0, sizeof(ClientSession));
        client->socket_fd = client_fd;
        client->state = STATE_CONNECTED;
        client->active = 1;
        client->last_activity = time(NULL);

        // create thread to handle client
        pthread_create(&client->thread_id, NULL, handle_client, client);
        pthread_detach(client->thread_id);

        pthread_mutex_unlock(&server->clients_mutex);
    }
    log_event(LOG_INFO, NULL, "SERVER", "Server shutting down main loop");
}

/**
 * @brief Handle client connection
 */
void *handle_client(void *arg)
{
    ClientSession *client = (ClientSession *)arg;
    char buffer[MAX_MESSAGE_LEN];

    printf("[Thread %lu] Handling client socket %d\n", pthread_self(), client->socket_fd);
    while (client->active && g_server->running)
    {
        memset(buffer, 0, sizeof(buffer));

        // receive header line
        int bytes_received = recv_line(client->socket_fd, buffer, sizeof(buffer));
        if (bytes_received <= 0)
        {
            printf("[Thread %lu] Client socket %d disconnected or error\n", pthread_self(), client->socket_fd);
            log_event(LOG_INFO, client->username[0] ? client->username : "anonymous", "DISCONNECT", "Client socket %d disconnected", client->socket_fd);
            break;
        }

        // check if data message
        // CODE DATA length\n
        if (strstr(buffer, " DATA "))
        { // -> buffer = " DATA length\n"
            // extract length and receive full data
            char temp[256];
            strncpy(temp, buffer, sizeof(temp) - 1);
            char *nl = strchr(temp, '\n');
            if (nl)
                *nl = '\0'; // -> temp = " DATA length"

            char *length_str = strrchr(temp, ' '); // -> length_str = " length"
            if (!length_str)
            {
                send_error_or_response(client->socket_fd, CODE_SYNTAX_ERROR, "Invalid DATA message format");
                continue;
            }

            size_t length = (size_t)atoll(length_str + 1);
            if (length > MAX_DATA_SIZE)
            {
                send_error_or_response(client->socket_fd, CODE_SYNTAX_ERROR, "Data size too large");
                continue;
            }

            char *full_buffer = (char *)malloc(bytes_received + length + 1);
            if (!full_buffer)
            {
                send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Memory allocation failed");
                continue;
            }

            memcpy(full_buffer, buffer, bytes_received);
            if (recv_full(client->socket_fd, full_buffer + bytes_received, length) < 0)
            {
                free(full_buffer);
                break;
            }

            full_buffer[bytes_received + length] = '\0';

            Message msg;
            if (parse_message(full_buffer, &msg) < 0)
            {
                send_error_or_response(client->socket_fd, CODE_SYNTAX_ERROR, "Invalid message format");
                free(full_buffer);
                continue;
            }

            free(full_buffer);
            free_message(&msg);
            continue;
        }

        // parse control message
        client->last_activity = time(NULL);
        Message msg;
        if (parse_message(buffer, &msg) < 0)
        {
            send_error_or_response(client->socket_fd, CODE_SYNTAX_ERROR, "Invalid message format");
            continue;
        }

        printf("[Thread %lu] Received command: %s\n", pthread_self(), msg.command);

        // handle commands
        if (strcmp(msg.command, MSG_REGISTER) == 0)
        {
            handle_register(g_server, client, &msg);
        }
        else if (strcmp(msg.command, MSG_LOGIN) == 0)
        {
            handle_login(g_server, client, &msg);
        }
        else if (strcmp(msg.command, MSG_LOGOUT) == 0)
        {
            handle_logout(g_server, client);
        }
        else if (strcmp(msg.command, MSG_LIST_ROOMS) == 0)
        {
            handle_list_rooms(g_server, client, &msg);
        }
        else if (strcmp(msg.command, MSG_CREATE_ROOM) == 0)
        {
            handle_create_room(g_server, client, &msg);
        }
        else if (strcmp(msg.command, MSG_PING) == 0)
        {
            send_error_or_response(client->socket_fd, CODE_PONG, "PONG");
        }
        else
        {
            send_error_or_response(client->socket_fd, CODE_BAD_COMMAND, msg.command);
            log_event(LOG_WARNING, client->username[0] ? client->username : "anonymous", "BAD_COMMAND", "Unknown command: %s", msg.command);
        }

        free_message(&msg);
    }

    // cleanup
    remove_client_session(g_server, client->socket_fd);
    close(client->socket_fd);
    client->active = 0;

    printf("[Thread %lu] Cleaning up client socket %d\n", pthread_self(), client->socket_fd);
    return NULL;
}

/**
 * @brief Find session by socket
 */
ClientSession *find_session_by_socket(Server *server, int socket_fd)
{
    pthread_mutex_lock(&server->clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (server->clients[i].active && server->clients[i].socket_fd == socket_fd)
        {
            pthread_mutex_unlock(&server->clients_mutex);
            return &server->clients[i];
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);
    return NULL;
}

/**
 * @brief Find session by username
 */
ClientSession *find_session_by_username(Server *server, const char *username)
{
    pthread_mutex_lock(&server->clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (server->clients[i].active && strcmp(server->clients[i].username, username) == 0)
        {
            pthread_mutex_unlock(&server->clients_mutex);
            return &server->clients[i];
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);
    return NULL;
}

/**
 * @brief Remove client session
 */
void remove_client_session(Server *server, int socket_fd)
{
    pthread_mutex_lock(&server->clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (server->clients[i].socket_fd == socket_fd)
        {
            if (strlen(server->clients[i].session_id) > 0)
            {
                db_destroy_session(server->db, server->clients[i].session_id);
                log_event(LOG_INFO, server->clients[i].username, "DISCONNECT", "Session destroyed");
            }
        }
        server->clients[i].active = 0;
        break;
    }

    pthread_mutex_unlock(&server->clients_mutex);
}

/**
 * @brief Send error/response to client
 */
void send_error_or_response(int socket_fd, int code, const char *message)
{
    char buffer[MAX_MESSAGE_LEN];
    int len = create_simple_response(code, message, buffer, sizeof(buffer));
    send_full(socket_fd, buffer, len);
}