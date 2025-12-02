#include "handle.h"
#include "../ui/ui.h"
#include "../protocol/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Handle user register
 */
void handle_register(Client *client) {
    char username[64], password[64];

    printf("=== REGISTER  ===\n");
    ui_get_input("Enter username (3-20 chars): ", username, sizeof(username));
    ui_get_input("Enter password (min 8 chars, upper, lower, digit): ", password, sizeof(password));

    // validate 
    if (!validate_username(username)) {
        ui_show_error("Invalid username format.");
        return;
    }
    if (!validate_password(password)) {
        ui_show_error("Invalid password format.");
        return;
    }

    // send REGISTER command    
    const char *params[] = {username, password};
    if (client_send_command(client, "REGISTER", params, 2) < 0) {
        ui_show_error("Failed to send REGISTER command.");
        return;
    }

    // receive response
    Response response;
    if (client_receive_response(client, &response) < 0) {
        ui_show_error("Failed to receive response from server.");
        return;
    }

    if(response.code == CODE_CREATED) {
        ui_show_success("Registration successful! You can now login.");
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "Registration failed: %s", response.message);
        ui_show_error(msg);
    }
}

/**
 * @brief Handle user login
 */
void handle_login(Client *client) {
    char username[64], password[64];

    printf("=== LOGIN ===\n");
    ui_get_input("Enter username: ", username, sizeof(username));
    ui_get_input("Enter password: ", password, sizeof(password));

    // send LOGIN command
    const char *params[] = {username, password};
    if (client_send_command(client, "LOGIN", params, 2) < 0) {
        ui_show_error("Failed to send LOGIN command.");
        return;
    }

    // receive response
    Response response;
    if (client_receive_response(client, &response) < 0) {
        ui_show_error("Failed to receive response from server.");
        return;
    }

    if (response.code == CODE_LOGIN_OK) {
        strncpy(client->username, username, sizeof(client->username) - 1);
        strncpy(client->session_id, response.message, sizeof(client->session_id) - 1);
        client->state = CLIENT_AUTHENTICATED;
        
        char msg[256];
        snprintf(msg, sizeof(msg), "Welcome back, %s!", username);
        ui_show_success(msg);
        
        printf("Session ID: %s\n", client->session_id);
    } else {
        char error[256];
        snprintf(error, sizeof(error), "[%d] %s", response.code, response.message);
        ui_show_error(error);
    }
}

/**
 * @brief Handle user logout
 */
void handle_logout(Client *client) {
    printf("\n=== LOGOUT ===\n");
    
    // Send command
    if (client_send_command(client, "LOGOUT", NULL, 0) < 0) {
        ui_show_error("Failed to send command");
        return;
    }
    
    // Receive response
    Response response;
    if (client_receive_response(client, &response) < 0) {
        ui_show_error("Failed to receive response");
        return;
    }
    
    if (response.code == CODE_LOGOUT_OK) {
        ui_show_success("Logged out successfully. Goodbye!");
        client->state = CLIENT_CONNECTED;
        memset(client->username, 0, sizeof(client->username));
        memset(client->session_id, 0, sizeof(client->session_id));
    } else {
        char error[256];
        snprintf(error, sizeof(error), "[%d] %s", response.code, response.message);
        ui_show_error(error);
    }
}
