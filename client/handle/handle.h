#ifndef HANDLE_H
#define HANDLE_H

#include "../client.h"

/**
 * @brief Handle user registration
 * @param client Client instance
 * 
 * Flow:
 * 1. Get username and password from user
 * 2. Send REGISTER command to server
 * 3. Receive and display response
 */
void handle_register(Client *client);

/**
 * @brief Handle user login
 * @param client Client instance
 * 
 * Flow:
 * 1. Get username and password from user
 * 2. Send LOGIN command to server
 * 3. Receive response
 * 4. Update client state if successful
 */
void handle_login(Client *client);

/**
 * @brief Handle user logout
 * @param client Client instance
 * 
 * Flow:
 * 1. Send LOGOUT command to server
 * 2. Clear client session data
 * 3. Update state to CONNECTED
 */
void handle_logout(Client *client);

#endif // HANDLE_H