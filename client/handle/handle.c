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
    if (client_create_send_command(client, "REGISTER", params, 2) < 0)
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
    if (client_create_send_command(client, "LOGIN", params, 2) < 0)
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
    if (client_create_send_command(client, "LOGOUT", NULL, 0) < 0)
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
 * @param client Client instance
 * Flow:
 * 1. Get filter parameter (ALL, NOT_STARTED, IN_PROGRESS, FINISHED)
 * 2. Send LIST_ROOMS command: LIST_ROOMS <filter>
 * 3. Receive and display room list (JSON): CODE_ROOMS_DATA <length>\n<JSON>
 */
// Filter: NOT_STARTED, IN_PROGRESS, FINISHED, ALL
// Command: LIST_ROOMS [filter]\n
// Response: 140 DATA <length>\n<data>
void handle_list_rooms(Client *client)
{
    char filter[32];
    char input_line[32];
    int choice = -1;

    ui_print_list_room_filter();

    // Read entire line, including newline character, enter newline = filter ALL
    if (fgets(input_line, sizeof(input_line), stdin) != NULL)
    {
        // Try to parse as integer
        if (sscanf(input_line, "%d", &choice) != 1) // not a number, sscanf fails
        {
            choice = 0; // Default to ALL if not a number or empty
        }
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

    // ========================== Send command ============================================
    const char *params[] = {filter};

    // only send filter if not ALL
    int param_count = (strcmp(filter, "ALL") == 0) ? 0 : 1;
    if (client_create_send_command(client, "LIST_ROOMS", params, param_count) < 0)
    {
        ui_show_error("Failed to send command");
        return;
    }

    // ========================== Receive response ============================================
    Response resp;
    if (client_receive_response(client, &resp) < 0)
    {
        ui_show_error("Failed to receive response");
        return;
    }

    if (resp.code == CODE_ROOMS_DATA && resp.data)
    {
        printf("\n=== AVAILABLE ROOMS ===\n");
        printf("Filter: %s\n", filter);
        printf("Data received: %zu bytes\n\n", resp.data_length);
        printf("%s\n", resp.data);

        free(resp.data); // free allocated data buffer to avoid memory leak
    }
    else
    {
        char error[256];
        snprintf(error, sizeof(error), "[%d] %s", resp.code, resp.message);
        ui_show_error(error);
    }
}
/**
 * @brief Handle create room
 */
// command: CREATE_ROOM room_name|num_questions|time_limit\n
void handle_create_room(Client *client)
{
    char room_name[128];
    char num_questions_str[16];
    char time_minutes_str[16];

    printf("\n=== CREATE ROOM ===\n");
    ui_get_input("Room name: ", room_name, sizeof(room_name));
    ui_get_input("Number of questions (5-50): ", num_questions_str, sizeof(num_questions_str));
    ui_get_input("Time limit (minutes, 5-120): ", time_minutes_str, sizeof(time_minutes_str));

    // ========================== Validate parameters ============================================
    int num_questions = atoi(num_questions_str);
    int time_minutes = atoi(time_minutes_str);

    if (num_questions < 5 || num_questions > 50)
    {
        ui_show_error("Number of questions must be between 5 and 50");
        return;
    }

    if (time_minutes < 5 || time_minutes > 120)
    {
        ui_show_error("Time limit must be between 5 and 120 minutes");
        return;
    }

    // ========================== Send command ============================================
    const char *params[] = {room_name, num_questions_str, time_minutes_str};
    if (client_create_send_command(client, "CREATE_ROOM", params, 3) < 0)
    {
        ui_show_error("Failed to send command");
        return;
    }

    // ========================== Receive response ============================================
    Response resp;
    if (client_receive_response(client, &resp) < 0)
    {
        ui_show_error("Failed to receive response");
        return;
    }

    if (resp.code == CODE_ROOM_CREATED)
    {
        // The server returns the room ID in resp.message and the client enters the room as creator
        snprintf(client->current_room, sizeof(client->current_room), "%s", resp.message);
        client->state = CLIENT_IN_ROOM;
        client->is_creator = 1; // Mark as creator

        char msg[256];
        snprintf(msg, sizeof(msg), "Room created successfully!");
        ui_show_success(msg);
        printf("Room ID: %s\n", resp.message);
        printf("You are now in the room as creator.\n");
    }
    else
    {
        char error[256];
        snprintf(error, sizeof(error), "[%d] %s", resp.code, resp.message);
        ui_show_error(error);
    }
}

/**
 * @brief Handle join room
 */
void handle_join_room(Client *client)
{
    char room_id[64];

    printf("\n=== JOIN ROOM ===\n");
    ui_get_input("Room ID: ", room_id, sizeof(room_id));

    if (strlen(room_id) == 0)
    {
        ui_show_error("Room ID cannot be empty");
        return;
    }

    // ========================== Send command ============================================
    const char *params[] = {room_id};
    if (client_create_send_command(client, "JOIN_ROOM", params, 1) < 0)
    {
        ui_show_error("Failed to send command");
        return;
    }

    // ========================== Receive response ============================================
    Response resp;
    if (client_receive_response(client, &resp) < 0)
    {
        ui_show_error("Failed to receive response");
        return;
    }

    // ========================== Handle response ============================================

    // Successful join
    if (resp.code == CODE_ROOM_JOIN_OK)
    {
        snprintf(client->current_room, sizeof(client->current_room), "%s", room_id);
        client->state = CLIENT_IN_ROOM;
        client->is_creator = 0; // Mark as participant

        char msg[256];
        snprintf(msg, sizeof(msg), "Successfully joined room: %s", room_id);
        ui_show_success(msg);
        ui_show_info("Waiting for the creator to start the exam...");
    }

    // Room not found
    else if (resp.code == CODE_ROOM_NOT_FOUND)
    {
        char error[256];
        snprintf(error, sizeof(error), "Room not found!\n");
        ui_show_error(error);
    }

    // Room already started
    else if (resp.code == CODE_ROOM_IN_PROGRESS)
    {
        char error[256];
        snprintf(error, sizeof(error), "Room already started!\n");
        ui_show_error(error);
    }

    // Room finished
    else if (resp.code == CODE_ROOM_FINISHED)
    {
        char error[256];
        snprintf(error, sizeof(error), "Room exam finished!\n");
        ui_show_error(error);
    }

    else
    {
        char error[256];
        snprintf(error, sizeof(error), "[%d] %s", resp.code, resp.message);
        ui_show_error(error);
    }
}

/**
 * @brief Handle view result
 */
void handle_view_result(Client *client)
{
    char room_id[64];

    printf("\n=== VIEW RESULT ===\n");
    ui_get_input("Room ID: ", room_id, sizeof(room_id));

    if (strlen(room_id) == 0)
    {
        ui_show_error("Room ID cannot be empty");
        return;
    }

    // send command = "VIEW_RESULT room_id\n"
    const char *params[] = {room_id};
    if (client_create_send_command(client, "VIEW_RESULT", params, 1) < 0)
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

    if (resp.code == CODE_RESULT_DATA && resp.data)
    {
        printf("\n=== EXAM RESULTS - LEADERBOARD ===\n");
        printf("Room: %s\n", room_id);
        printf("Data received: %zu bytes\n\n", resp.data_length);
        printf("%s\n", resp.data);
        free(resp.data);
    }
    else if (resp.code == CODE_NOT_LOGGED)
    {
        ui_show_error("You are not logged in.");
        return;
    }
    else if (resp.code == CODE_ROOM_IN_PROGRESS)
    {
        ui_show_error("The room exam is not started or still in progress.");
        return;
    }
    else if (resp.code == CODE_ROOM_NOT_FOUND)
    {
        ui_show_error("Room not found.");
        return;
    }
    else
    {
        char error[256];
        snprintf(error, sizeof(error), "[%d] %s", resp.code, resp.message);
        ui_show_error(error);
    }
}

/**
 * @brief Handle leave room
 */
void handle_leave_room(Client *client)
{
    printf("\n=== LEAVE ROOM ===\n");

    if (strlen(client->current_room) == 0)
    {
        ui_show_error("You are not in any room");
        return;
    }

    // Send command
    const char *params[] = {client->current_room};
    if (client_create_send_command(client, "LEAVE_ROOM", params, 1) < 0)
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

    if (resp.code == CODE_ROOM_LEAVE_OK)
    {
        ui_show_success("Left the room successfully");
        client->state = CLIENT_AUTHENTICATED;
        memset(client->current_room, 0, sizeof(client->current_room)); // Clear current room
        client->is_creator = 0;                                        // Reset creator flag
    }
    else
    {
        char error[256];
        snprintf(error, sizeof(error), "[%d] %s", resp.code, resp.message);
        ui_show_error(error);
    }
} /**
   * @brief Handle start exam
   */
void handle_start_exam(Client *client)
{
    printf("\n=== START EXAM ===\n");

    if (strlen(client->current_room) == 0)
    {
        ui_show_error("You are not in any room");
        return;
    }

    ui_show_info("Starting exam...");

    // Send command
    const char *params[] = {client->current_room};
    if (client_create_send_command(client, "START_EXAM", params, 1) < 0)
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

    if (resp.code == CODE_START_OK)
    {
        client->state = CLIENT_IN_EXAM;
        ui_show_success("Exam started!");
        printf("Start time: %s\n", resp.message);
        ui_show_info("All participants have been notified");
    }
    else
    {
        char error[256];
        snprintf(error, sizeof(error), "[%d] %s", resp.code, resp.message);
        ui_show_error(error);
    }
}

/**
 * @brief Handle GET_EXAM - fetch exam questions
 */
void handle_get_exam(Client *client)
{
    printf("\n=== GET EXAM ===\n");

    if (strlen(client->current_room) == 0)
    {
        ui_show_error("You are not in any room");
        return;
    }

    ui_show_info("Fetching exam questions...");

    // Send GET_EXAM command
    const char *params[] = {client->current_room};
    if (client_create_send_command(client, "GET_EXAM", params, 1) < 0)
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

    if (resp.code == 150) // CODE_EXAM_DATA
    {
        ui_show_success("Exam questions received!");
        printf("\n========================================\n");
        printf("EXAM QUESTIONS\n");
        printf("========================================\n");
        printf("%s\n", resp.data);
        printf("========================================\n");
        printf("\nYou can now answer the questions. Choose your answers and submit in the next step.\n");
    }
    else
    {
        char error[256];
        snprintf(error, sizeof(error), "[%d] %s", resp.code, resp.message);
        ui_show_error(error);
    }
}

/**
 * @brief Handle submit exam
 */
void handle_submit_exam(Client *client)
{
    printf("\n=== SUBMIT EXAM ===\n");

    if (strlen(client->current_room) == 0)
    {
        ui_show_error("You are not in any room");
        return;
    }

    // Get answers from user
    char input[512];
    printf("Enter your answers (e.g., A,B,C,D,A or A B C D A):\n");
    printf("Answers: ");

    if (fgets(input, sizeof(input), stdin) == NULL)
    {
        ui_show_error("Failed to read answers");
        return;
    }

    // Remove newline
    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n')
    {
        input[len - 1] = '\0';
    }

    // Validate not empty
    if (strlen(input) == 0)
    {
        ui_show_error("Answers cannot be empty");
        return;
    }

    // Process input: remove spaces and convert to comma-separated format
    char answers[512] = "";
    int answer_count = 0;
    char *token = strtok(input, " ,"); // Split by space or comma

    while (token != NULL)
    {
        // Validate answer is A, B, C, or D
        if (strlen(token) == 1 && (token[0] == 'A' || token[0] == 'B' ||
                                   token[0] == 'C' || token[0] == 'D' ||
                                   token[0] == 'a' || token[0] == 'b' ||
                                   token[0] == 'c' || token[0] == 'd'))
        {
            // Convert to uppercase
            char upper = (token[0] >= 'a' && token[0] <= 'z') ? token[0] - 32 : token[0];

            if (answer_count > 0)
            {
                strcat(answers, ",");
            }
            strncat(answers, &upper, 1);
            answer_count++;
        }
        else
        {
            char error[256];
            snprintf(error, sizeof(error), "Invalid answer '%s'. Use A, B, C, or D only.", token);
            ui_show_error(error);
            return;
        }

        token = strtok(NULL, " ,");
    }

    if (answer_count == 0)
    {
        ui_show_error("No valid answers found");
        return;
    }

    ui_show_info("Submitting exam...");

    // Send SUBMIT_EXAM command: room_id|answers
    const char *params[] = {client->current_room, answers};
    if (client_create_send_command(client, "SUBMIT_EXAM", params, 2) < 0)
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

    if (resp.code == 130) // CODE_SUBMIT_OK
    {
        ui_show_success("Exam submitted successfully!");
        printf("\n========================================\n");
        printf("YOUR RESULT\n");
        printf("========================================\n");

        // Parse score|total from message
        char *score_str = strtok(resp.message, "|");
        char *total_str = strtok(NULL, "|");

        if (score_str && total_str)
        {
            int score = atoi(score_str);
            int total = atoi(total_str);
            double percentage = (total > 0) ? (score * 100.0 / total) : 0.0;

            printf("Score: %d/%d (%.1f%%)\n", score, total, percentage);

            if (percentage >= 80)
            {
                printf("Excellent! Well done!\n");
            }
            else if (percentage >= 60)
            {
                printf("Good job! Keep it up!\n");
            }
            else if (percentage >= 40)
            {
                printf("Not bad, but you can do better!\n");
            }
            else
            {
                printf("Keep practicing!\n");
            }
        }
        else
        {
            printf("Result: %s\n", resp.message);
        }

        printf("========================================\n");

        // Update client state - exit exam
        client->state = CLIENT_AUTHENTICATED;
        memset(client->current_room, 0, sizeof(client->current_room));

        printf("\nYou have been returned to the main menu.\n");
    }
    else if (resp.code == 131) // CODE_ALREADY_SUBMITTED
    {
        ui_show_info("You have already submitted this exam!");
        printf("\n========================================\n");
        printf("YOUR PREVIOUS RESULT\n");
        printf("========================================\n");

        // Parse score|total from message
        char *score_str = strtok(resp.message, "|");
        char *total_str = strtok(NULL, "|");

        if (score_str && total_str)
        {
            int score = atoi(score_str);
            int total = atoi(total_str);
            double percentage = (total > 0) ? (score * 100.0 / total) : 0.0;

            printf("Score: %d/%d (%.1f%%)\n", score, total, percentage);
        }
        else
        {
            printf("Result: %s\n", resp.message);
        }

        printf("========================================\n");
    }
    else
    {
        char error[256];
        snprintf(error, sizeof(error), "[%d] %s", resp.code, resp.message);
        ui_show_error(error);
    }
}
