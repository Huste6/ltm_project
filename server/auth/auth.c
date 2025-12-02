#include "auth.h"
#include "../server.h"
#include <openssl/sha.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * @brief Hash password bằng SHA256
 */
void sha256_hash(const char *input, char output[65]) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)input, strlen(input), hash);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
    output[64] = '\0';
}

/**
 * @brief Check authentication
 */
int check_authentication(ClientSession *client) {
    return (client->state >= STATE_AUTHENTICATED) && strlen(client->session_id) > 0;
}

/**
 * @brief Handle REGISTER command
 */
void handle_register(Server *server, ClientSession *client, Message *msg) {
    // check params 
    if (msg->param_count < 2) {
        send_error_or_response(client->socket_fd, CODE_SYNTAX_ERROR, "Usage: REGISTER username|password");
        return;
    }

    const char *username = msg->params[0];
    const char *password = msg->params[1];

    // validate username
    if (!validate_username(username)) {
        send_error_or_response(client->socket_fd, CODE_INVALID_USERNAME, "Username: 3-20 chars, alphanumeric + underscore only");
        db_log_activity(server->db, "WARNING", username, "REGISTER", "Invalid username format");
        return;
    }
    // validate password
    if (!validate_password(password)) {
        send_error_or_response(client->socket_fd, CODE_WEAK_PASSWORD, "Password: min 8 chars, at least 1 upper, 1 lower, 1 digit");
        db_log_activity(server->db, "WARNING", username, "REGISTER", "Weak password");
        return;
    }
    // check username exists
    if (db_check_username_exists(server->db, username)) {
        send_error_or_response(client->socket_fd, CODE_USERNAME_EXISTS, "Username already exists");
        db_log_activity(server->db, "WARNING", username, "REGISTER", "Username already exists");
        return;
    }
    // hash password
    char password_hash[65];
    sha256_hash(password, password_hash);

    // create user in DB
    if (db_create_user(server->db, username, password_hash) < 0) {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Internal server error");
        db_log_activity(server->db, "ERROR", username, "REGISTER", "Database error on user creation");
        return;
    }

    // success
    send_error_or_response(client->socket_fd, CODE_CREATED, "Account created successfully");
    db_log_activity(server->db, "INFO", username, "REGISTER", "Account created successfully");

    printf("[REGISTER] User '%s' registered successfully.\n", username);
}

/**
 * @brief Handle LOGIN command
 */
void handle_login(Server *server, ClientSession *client, Message *msg) {
    // Check params
    if (msg->param_count < 2) {
        send_error_or_response(client->socket_fd, CODE_SYNTAX_ERROR, "Usage: LOGIN username|password");
        return;
    }
    
    const char *username = msg->params[0];
    const char *password = msg->params[1];
    
    // Hash password
    char password_hash[65];
    sha256_hash(password, password_hash);
    
    // Check account exists
    if (!db_check_username_exists(server->db, username)) {
        send_error_or_response(client->socket_fd, CODE_ACCOUNT_NOT_FOUND, "Account not found");
        db_log_activity(server->db, "WARNING", username, "LOGIN", "Account not found");
        return;
    }
    
    // Verify credentials
    if (!db_verify_login(server->db, username, password_hash)) {
        send_error_or_response(client->socket_fd, CODE_WRONG_PASSWORD, "Invalid credentials");
        db_log_activity(server->db, "WARNING", username, "LOGIN", "Wrong password");
        return;
    }
    
    // Check account not locked
    if (db_is_account_locked(server->db, username)) {
        send_error_or_response(client->socket_fd, CODE_ACCOUNT_LOCKED, "Account is locked");
        db_log_activity(server->db, "WARNING", username, "LOGIN", "Account locked");
        return;
    }
    
    // Check if already logged in
    if (db_check_user_logged_in(server->db, username)) {
        send_error_or_response(client->socket_fd, CODE_ALREADY_LOGGED, "User already logged in else where");
        db_log_activity(server->db, "WARNING", username, "LOGIN", "Already logged in else where");
        return;
    }
    
    // Generate session ID
    char session_id[MAX_SESSION_ID_LEN];
    snprintf(session_id, sizeof(session_id), "sess_%ld_%s", time(NULL), username);
    
    // Create session in database
    if (db_create_session(server->db, session_id, username) < 0) {
        send_error_or_response(client->socket_fd, CODE_INTERNAL_ERROR, "Failed to create session");
        db_log_activity(server->db, "ERROR", username, "LOGIN", "Session creation failed");
        return;
    }
    
    // Update client session
    strcpy(client->session_id, session_id);
    strcpy(client->username, username);
    client->state = STATE_AUTHENTICATED;
    
    // Send response
    send_error_or_response(client->socket_fd, CODE_LOGIN_OK, session_id);
    db_log_activity(server->db, "INFO", username, "LOGIN", "Successful login");
    
    printf("[LOGIN] User '%s' logged in with session %s\n", username, session_id);
}

/**
 * @brief Handle LOGOUT command
 */
void handle_logout(Server *server, ClientSession *client) {
    // Check authentication
    if (!check_authentication(client)) {
        send_error_or_response(client->socket_fd, CODE_NOT_LOGGED, "Not authenticated");
        return;
    }
    
    // Destroy session
    db_destroy_session(server->db, client->session_id);
    db_log_activity(server->db, "INFO", client->username, "LOGOUT", "Logged out");
    
    printf("[LOGOUT] User '%s' logged out\n", client->username);
    
    // Clear client session
    memset(client->session_id, 0, sizeof(client->session_id));
    memset(client->username, 0, sizeof(client->username));
    client->state = STATE_CONNECTED;
    
    // Send response
    send_error_or_response(client->socket_fd, CODE_LOGOUT_OK, "Goodbye");
}