#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/**
 * @brief Connect to server
 */
int client_connect(Client *client, const char *host, int port) {
    memset(client, 0, sizeof(Client));

    // create socket
    client->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(client->socket_fd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    // setup server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if(inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(client->socket_fd);
        return -1;
    }

    // connect to server
    if(connect(client->socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection to server failed");
        close(client->socket_fd);
        return -1;
    }

    client->state = CLIENT_CONNECTED;
    return 0;
}

/**
 * @brief Disconnect from server
 */
void client_disconnect(Client *client) {
    if(client->socket_fd > 0) {
        close(client->socket_fd);
        client->socket_fd = -1;
    }
    client->state = CLIENT_DISCONNECTED;
}

/**
 * @brief Send command to server
 */
int client_send_command(Client *client, const char *command, const char **params, int param_count) {
    char buffer[BUFFER_SIZE];
    int len = create_control_message(command, params, param_count, buffer, sizeof(buffer));
    if(len <= 0){
        fprintf("Failed to create command message\n");
        return -1;
    }

    return send_full(client->socket_fd, buffer, len);
}

/**
 * @brief Receive response from server
 */
int client_receive_response(Client *client, Response *response) {
    char buffer[BUFFER_SIZE];
    memset(response, 0, sizeof(Response));

    // receive header line
    int bytes_received = recv_line(client->socket_fd, buffer, sizeof(buffer));
    if(bytes_received <= 0) {
        fprintf(stderr, "Connection lost\n");
        return -1;
    }

    // check if data message
    if (strstr(buffer, " DATA ") != NULL) {
        // Parse header to get length
        char temp[256];
        strncpy(temp, buffer, sizeof(temp) - 1);
        char *nl = strchr(temp, '\n');
        if (nl) *nl = '\0';
        
        // Extract code and length
        int code;
        size_t data_len;
        sscanf(temp, "%d DATA %zu", &code, &data_len);
        
        response->code = code;
        response->data_len = data_len;
        
        // Receive data
        response->data = malloc(data_len + 1);
        if (!response->data) {
            fprintf(stderr, "Memory allocation failed\n");
            return -1;
        }
        
        int received = recv_full(client->socket_fd, response->data, data_len);
        if (received < 0) {
            free(response->data);
            return -1;
        }
        
        response->data[data_len] = '\0';
        response->message[0] = '\0';
        
    } else {
        // Simple response: CODE MESSAGE\n
        char temp[256];
        strncpy(temp, buffer, sizeof(temp) - 1);
        char *nl = strchr(temp, '\n');
        if (nl) *nl = '\0';
        
        // Parse code
        char *space = strchr(temp, ' ');
        if (space) {
            *space = '\0';
            response->code = atoi(temp);
            strncpy(response->message, space + 1, sizeof(response->message) - 1);
        } else {
            response->code = atoi(temp);
            response->message[0] = '\0';
        }
        
        response->data = NULL;
        response->data_len = 0;
    }
    
    return 0;
}
