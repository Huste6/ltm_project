#include "handle.h"
#include "../ui/ui.h"
#include "../protocol/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Handle user register
 */
void handle_register(Client *client)
{
    char username[64], password[64];

    printf("=== REGISTER  ===\n");
    ui_get_input("Enter username (3-20 chars): ", username, sizeof(username));
    ui_get_input("Enter password (min 8 chars, upper, lower, digit): ", password, sizeof(password));

    // validate
    if (!validate_username(username))
    {
        ui_show_error("Invalid username format.");
        return;
    }
    if (!validate_password(password))
    {
        ui_show_error("Invalid password format.");
        return;
    }

    // send REGISTER command
    const char *params[] = {username, password};
    if (client_send_command(client, "REGISTER", params, 2) < 0)
    {
        ui_show_error("Failed to send REGISTER command.");
        return;
    }

    // receive response
    Response response;
    if (client_receive_response(client, &response) < 0)
    {
        ui_show_error("Failed to receive response from server.");
        return;
    }

    if (response.code == CODE_CREATED)
    {
        ui_show_success("Registration successful! You can now login.");
    }
    else
    {
        char msg[256];
        snprintf(msg, sizeof(msg), "Registration failed: %s", response.message);
        ui_show_error(msg);
    }
}

/**
 * @brief Handle user login
 */
void handle_login(Client *client)
{
    char username[64], password[64];

    printf("=== LOGIN ===\n");
    ui_get_input("Enter username: ", username, sizeof(username));
    ui_get_input("Enter password: ", password, sizeof(password));

    // send LOGIN command
    const char *params[] = {username, password};
    if (client_send_command(client, "LOGIN", params, 2) < 0)
    {
        ui_show_error("Failed to send LOGIN command.");
        return;
    }

    // receive response
    Response response;
    if (client_receive_response(client, &response) < 0)
    {
        ui_show_error("Failed to receive response from server.");
        return;
    }

    if (response.code == CODE_LOGIN_OK)
    {
        strncpy(client->username, username, sizeof(client->username) - 1);
        strncpy(client->session_id, response.message, sizeof(client->session_id) - 1);
        client->state = CLIENT_AUTHENTICATED;

        char msg[256];
        snprintf(msg, sizeof(msg), "Welcome back, %s!", username);
        ui_show_success(msg);

        printf("Session ID: %s\n", client->session_id);
    }
    else
    {
        char error[256];
        snprintf(error, sizeof(error), "[%d] %s", response.code, response.message);
        ui_show_error(error);
    }
}

/**
 * @brief Handle user logout
 */
void handle_logout(Client *client)
{
    printf("\n=== LOGOUT ===\n");

    // Send command
    if (client_send_command(client, "LOGOUT", NULL, 0) < 0)
    {
        ui_show_error("Failed to send command");
        return;
    }

    // Receive response
    Response response;
    if (client_receive_response(client, &response) < 0)
    {
        ui_show_error("Failed to receive response");
        return;
    }

    if (response.code == CODE_LOGOUT_OK)
    {
        ui_show_success("Logged out successfully. Goodbye!");
        client->state = CLIENT_CONNECTED;
        memset(client->username, 0, sizeof(client->username));
        memset(client->session_id, 0, sizeof(client->session_id));
    }
    else
    {
        char error[256];
        snprintf(error, sizeof(error), "[%d] %s", response.code, response.message);
        ui_show_error(error);
    }
}

/**
 * @brief Handle list rooms
 */
void handle_list_rooms(Client *client)
{
    char filter[32];
    char input_line[32];
    int choice = -1;

    printf("\n=== LIST ROOMS ===\n");
    printf("Select filter:\n");
    printf("1. NOT_STARTED\n");
    printf("2. IN_PROGRESS\n");
    printf("3. FINISHED\n");
    printf("0. ALL (default)\n");
    printf("Choice (press Enter for ALL): ");
    fflush(stdout);

    // Read entire line
    if (fgets(input_line, sizeof(input_line), stdin) != NULL)
    {
        // Try to parse as integer
        if (sscanf(input_line, "%d", &choice) != 1)
        {
            choice = 0; // Default to ALL if not a number or empty
        }
    }
    else
    {
        choice = 0; // Default to ALL on EOF
    }

    switch (choice)
    {
    case 1:
        strcpy(filter, "NOT_STARTED");
        break;
    case 2:
        strcpy(filter, "IN_PROGRESS");
        break;
    case 3:
        strcpy(filter, "FINISHED");
        break;
    case 0:
    default:
        strcpy(filter, "ALL");
        break;
    }

    // Send command
    const char *params[] = {filter};
    int param_count = (strcmp(filter, "ALL") == 0) ? 0 : 1;
    if (client_send_command(client, "LIST_ROOMS", params, param_count) < 0)
    {
        ui_show_error("Failed to send command");
        return;
    }

    // Receive response
    Response resp;
    if (client_receive_response(client, &resp) < 0)
    {
        ui_show_error("Failed to receive response");
        return;
    }

    if (resp.code == CODE_ROOMS_DATA && resp.data)
    {
        printf("\n=== AVAILABLE ROOMS ===\n");
        printf("Filter: %s\n\n", filter);

        // Parse and display JSON data
        // Simple parsing: look for room_id fields
        printf("%-12s | %-20s | %-12s | %-12s | %-6s\n", 
               "Room ID", "Room Name", "Creator", "Status", "Users");
        printf("%-12s-+-%-20s-+-%-12s-+-%-12s-+-%-6s\n", 
               "------------", "--------------------", "------------", "------------", "------");

        // Parse JSON manually (simple approach)
        const char *json = resp.data;
        const char *room_start = json;
        int room_count = 0;

        // Count rooms in JSON by counting "room_id" fields
        while ((room_start = strstr(room_start, "\"room_id\"")) != NULL) {
            room_count++;
            room_start++;
        }

        if (room_count == 0) {
            printf("No rooms found.\n");
        } else {
            // Simple extraction of key fields
            // Note: For production, use a proper JSON library like cJSON
            char room_id[32], room_name[64], creator[32], status[32];
            const char *pos = json;

            for (int i = 0; i < room_count; i++) {
                // Extract room_id
                if (sscanf(pos, "%*[^:]:\"%31[^\"]\"", room_id) != 1) break;
                
                // Find room_name
                pos = strstr(pos, "room_name");
                if (!pos) break;
                if (sscanf(pos, "%*[^:]:\"%63[^\"]\"", room_name) != 1) break;
                
                // Find creator
                pos = strstr(pos, "creator");
                if (!pos) break;
                if (sscanf(pos, "%*[^:]:\"%31[^\"]\"", creator) != 1) break;
                
                // Find status
                pos = strstr(pos, "\"status\"");
                if (!pos) break;
                if (sscanf(pos, "%*[^:]:\"%31[^\"]\"", status) != 1) break;

                // Display room info
                printf("%-12s | %-20s | %-12s | %-12s | %-6d\n",
                       room_id, room_name, creator, status, 1);

                // Move to next room
                pos = strstr(pos, "},");
                if (!pos) break;
                pos++;
            }
        }

        printf("\nTotal: %d room(s) found\n", room_count);
        free(resp.data);
    }
    else
    {
        char error[256];
        snprintf(error, sizeof(error), "[%d] %s", resp.code, resp.message);
        ui_show_error(error);
    }
}
