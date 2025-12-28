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
    else if (response.code == CODE_USERNAME_EXISTS)
    {
        ui_show_error("Username already exists. Please choose another username.");
    }
    else if (response.code == CODE_INVALID_USERNAME)
    {
        ui_show_error("Invalid username format. Username must be 3-20 characters, alphanumeric and underscore only.");
    }
    else if (response.code == CODE_WEAK_PASSWORD)
    {
        ui_show_error("Weak password. Password must be at least 8 characters with uppercase, lowercase, and digit.");
    }
    else if (response.code == CODE_SYNTAX_ERROR)
    {
        ui_show_error("Syntax error. Please try again.");
    }
    else if (response.code == CODE_INTERNAL_ERROR)
    {
        ui_show_error("Internal server error. Please try again later.");
    }
    else
    {
        char msg[512];
        snprintf(msg, sizeof(msg), "Registration failed [%d]: %s", response.code, response.message);
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
    else if (response.code == CODE_ACCOUNT_NOT_FOUND)
    {
        ui_show_error("Account not found. Please register first.");
    }
    else if (response.code == CODE_WRONG_PASSWORD)
    {
        ui_show_error("Invalid credentials. Wrong password.");
    }
    else if (response.code == CODE_ACCOUNT_LOCKED)
    {
        ui_show_error("Your account is locked. Please contact administrator.");
    }
    else if (response.code == CODE_ALREADY_LOGGED)
    {
        ui_show_error("User already logged in elsewhere. Please logout from other session first.");
    }
    else if (response.code == CODE_SYNTAX_ERROR)
    {
        ui_show_error("Syntax error. Please try again.");
    }
    else if (response.code == CODE_INTERNAL_ERROR)
    {
        ui_show_error("Internal server error. Please try again later.");
    }
    else
    {
        char error[512];
        snprintf(error, sizeof(error), "Login failed [%d]: %s", response.code, response.message);
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
    else if (response.code == CODE_NOT_LOGGED)
    {
        ui_show_error("You are not logged in.");
    }
    else if (response.code == CODE_SESSION_EXPIRED)
    {
        ui_show_error("Your session has expired. Please login again.");
        client->state = CLIENT_CONNECTED;
        memset(client->username, 0, sizeof(client->username));
        memset(client->session_id, 0, sizeof(client->session_id));
    }
    else
    {
        char error[512];
        snprintf(error, sizeof(error), "Logout failed [%d]: %s", response.code, response.message);
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
        char error[512];
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
    ui_get_input("Time limit (minutes, 1-120): ", time_minutes_str, sizeof(time_minutes_str));

    // ========================== Validate parameters ============================================
    int num_questions = atoi(num_questions_str);
    int time_minutes = atoi(time_minutes_str);

    if (num_questions < 5 || num_questions > 50)
    {
        ui_show_error("Number of questions must be between 5 and 50");
        return;
    }

    if (time_minutes < 1 || time_minutes > 120)
    {
        ui_show_error("Time limit must be between 1 and 120 minutes");
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
        char error[512];
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
        char error[512];
        snprintf(error, sizeof(error), "Room not found!\n");
        ui_show_error(error);
    }

    // Room already started
    else if (resp.code == CODE_ROOM_IN_PROGRESS)
    {
        char error[512];
        snprintf(error, sizeof(error), "Room already started!\n");
        ui_show_error(error);
    }

    // Room finished
    else if (resp.code == CODE_ROOM_FINISHED)
    {
        char error[512];
        snprintf(error, sizeof(error), "Room exam finished!\n");
        ui_show_error(error);
    }

    else
    {
        char error[512];
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
        char error[512];
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
        char error[512];
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
        char error[512];
        snprintf(error, sizeof(error), "[%d] %s", resp.code, resp.message);
        ui_show_error(error);
    }
}

/**
 * @brief Allow user to modify a saved answer
 */
void modify_answer(Client *client, int *question_ids, int total_questions)
{
    printf("\n=== MODIFY ANSWER ===\n");
    printf("Which question do you want to modify? (1-%d): ", total_questions);

    int q_num;
    if (scanf("%d", &q_num) != 1)
    {
        ui_show_error("Invalid input");
        getchar();
        return;
    }
    getchar();

    if (q_num < 1 || q_num > total_questions)
    {
        ui_show_error("Invalid question number");
        return;
    }

    // Get new answer
    char choice;
    int valid = 0;

    while (!valid)
    {
        printf("New answer (A/B/C/D): ");
        if (scanf(" %c", &choice) != 1)
        {
            ui_show_error("Failed to read input");
            getchar();
            continue;
        }
        getchar();

        if (choice >= 'a' && choice <= 'z')
            choice -= 32;

        if (choice >= 'A' && choice <= 'D')
            valid = 1;
        else
            printf("Invalid! Please use A, B, C, or D.\n");
    }

    // Send SAVE_ANSWER to overwrite
    int question_id = question_ids[q_num - 1]; // Map question number to actual question ID
    char question_id_str[16];
    char opt_str[2];

    snprintf(question_id_str, sizeof(question_id_str), "%d", question_id);
    opt_str[0] = choice;
    opt_str[1] = '\0';

    const char *save_params[] = {client->current_room, question_id_str, opt_str};

    if (client_create_send_command(client, "SAVE_ANSWER", save_params, 3) < 0)
    {
        ui_show_error("Failed to save answer");
        return;
    }

    Response save_resp;
    if (client_receive_response(client, &save_resp) < 0)
    {
        ui_show_error("Failed to receive response");
        return;
    }

    if (save_resp.code == 160)
    {
        printf("✓ Question %d updated to %c\n", q_num, choice);
    }
    else if (save_resp.code == 230 || save_resp.code == 231)
    {
        ui_show_error("Exam time expired. Cannot modify answer.");
    }
    else
    {
        char error[256];
        snprintf(error, sizeof(error), "Failed: [%d] %s", save_resp.code, save_resp.message);
        ui_show_error(error);
    }
}

/**
 * @brief Display exam questions from JSON data
 */
void display_exam_questions(const char *json_data, int *question_ids, int total_questions)
{
    printf("\n========================================\n");
    printf("EXAM QUESTIONS (%d questions)\n", total_questions);
    printf("========================================\n");

    const char *search = json_data;
    int q_num = 1;

    while ((search = strstr(search, "\"content\": \"")) != NULL && q_num <= total_questions)
    {
        const char *content_start = search + 12;
        const char *content_end = strstr(content_start, "\"");

        if (content_end)
        {
            printf("\nQuestion %d: ", q_num);
            fwrite(content_start, 1, content_end - content_start, stdout);
            printf("\n");

            // Find and display options (already formatted by server)
            const char *options_start = strstr(content_end, "\"options\": [");
            if (options_start)
            {
                options_start += 12;
                const char *option_end = strstr(options_start, "]");

                if (option_end)
                {
                    const char *opt_ptr = options_start;
                    while (opt_ptr < option_end)
                    {
                        const char *opt_start = strstr(opt_ptr, "\"");
                        if (!opt_start || opt_start >= option_end)
                            break;
                        opt_start++;

                        const char *opt_close = strstr(opt_start, "\"");
                        if (!opt_close || opt_close >= option_end)
                            break;

                        printf("  ");
                        fwrite(opt_start, 1, opt_close - opt_start, stdout);
                        printf("\n");

                        opt_ptr = opt_close + 1;
                    }
                }
            }

            q_num++;
            search = content_end + 1;
        }
        else
        {
            break;
        }
    }

    printf("========================================\n");
}

/**
 * @brief Answer exam questions - auto-save each answer
 * Client sends SAVE_ANSWER immediately for each question
 */
void handle_answer_questions(Client *client, const int *question_ids, int total_questions)
{
    printf("\n========================================\n");
    printf("ANSWER QUESTIONS\n");
    printf("========================================\n");

    for (int i = 0; i < total_questions; i++)
    {
        char choice;
        int valid = 0;

        // Keep asking until valid answer
        while (!valid)
        {
            printf("Answer for question %d (A/B/C/D): ", i + 1);
            if (scanf(" %c", &choice) != 1)
            {
                ui_show_error("Failed to read input");
                getchar();
                continue;
            }
            getchar();

            // Convert to uppercase
            if (choice >= 'a' && choice <= 'z')
                choice -= 32;

            // Validate
            if (choice >= 'A' && choice <= 'D')
            {
                valid = 1;
            }
            else
            {
                printf("Invalid! Please use A, B, C, or D.\n");
            }
        }

        // Send SAVE_ANSWER immediately
        char question_id_str[16];
        char opt_str[2];
        snprintf(question_id_str, sizeof(question_id_str), "%d", question_ids[i]);
        opt_str[0] = choice;
        opt_str[1] = '\0';

        const char *save_params[] = {client->current_room, question_id_str, opt_str};

        if (client_create_send_command(client, "SAVE_ANSWER", save_params, 3) < 0)
        {
            ui_show_error("Failed to save answer");
            continue;
        }

        Response save_resp;
        if (client_receive_response(client, &save_resp) < 0)
        {
            ui_show_error("Failed to receive save response");
            continue;
        }

        if (save_resp.code == 160) // CODE_ANSWER_SAVED
        {
            printf("✓ Answer %d saved\n", i + 1);
        }
        else if (save_resp.code == 230 || save_resp.code == 231)
        {
            // TIME_EXPIRED or INVALID_STATE - exam ended
            ui_show_error("Exam time expired. Your submitted answers will be graded by the server.");
            printf("\n========================================\n");
            printf("You have answered %d question(s).\n", i);
            printf("========================================\n");
            return;
        }
        else
        {
            char error[256];
            snprintf(error, sizeof(error), "Failed to save answer: [%d] %s",
                     save_resp.code, save_resp.message);
            ui_show_error(error);
        }
    }

    printf("\n========================================\n");
    printf("All answers saved!\n");
    printf("You can now submit the exam or modify answers.\n");
    printf("========================================\n");
}

/**
 * @brief Handle GET_EXAM - fetch and display exam questions
 * Flow:
 * 1. Check state
 * 2. Send GET_EXAM command
 * 3. Parse JSON to extract question_ids
 * 4. Display questions
 * 5. Collect answers (auto-save each one)
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

        // Parse JSON to extract question_ids
        int total_questions = 0;
        int question_ids[100];
        memset(question_ids, 0, sizeof(question_ids));

        const char *search = resp.data;
        while ((search = strstr(search, "\"question_id\": ")) != NULL && total_questions < 100)
        {
            question_ids[total_questions] = atoi(search + 15);
            total_questions++;
            search = search + 16;
        }

        if (total_questions <= 0)
        {
            ui_show_error("Failed to parse questions from server");
            return;
        }

        // Display questions
        display_exam_questions(resp.data, question_ids, total_questions);

        // Collect answers
        handle_answer_questions(client, question_ids, total_questions);

        // Show post-exam menu
        int post_exam_choice = -1;
        while (post_exam_choice != 0)
        {

            ui_print_menu_exam();
            if (scanf("%d", &post_exam_choice) != 1)
            {
                ui_show_error("Invalid input");
                getchar();
                continue;
            }
            getchar();

            switch (post_exam_choice)
            {
            case 1:
                handle_submit_exam(client);
                post_exam_choice = 0; // Exit menu after submit
                break;
            case 2:
                modify_answer(client, question_ids, total_questions);
                break;
            default:
                printf("Invalid choice!\n");
            }
        }

        free(resp.data);
    }
    else if (resp.code == 224) // CODE_ROOM_NOT_STARTED
    {
        ui_show_error("Room exam has not started yet");
    }
    else if (resp.code == 225) // CODE_ROOM_FINISHED
    {
        ui_show_error("Room exam has already finished");
    }
    else
    {
        char error[512];
        snprintf(error, sizeof(error), "[%d] %s", resp.code, resp.message);
        ui_show_error(error);
    }
}

/**
 * @brief Handle submit exam - User submits answers
 * Server automatically grades from SAVE_ANSWER buffer
 * Server auto-submits on timeout
 */
void handle_submit_exam(Client *client)
{
    printf("\n=== SUBMIT EXAM ===\n");

    if (strlen(client->current_room) == 0)
    {
        ui_show_error("You are not in any room");
        return;
    }

    ui_show_info("Submitting exam...");

    // Send SUBMIT_EXAM command
    const char *params[] = {client->current_room};
    if (client_create_send_command(client, "SUBMIT_EXAM", params, 1) < 0)
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
        display_exam_result(resp.message);

        // Return to menu
        client->state = CLIENT_AUTHENTICATED;
        memset(client->current_room, 0, sizeof(client->current_room));
        printf("\nYou have been returned to the main menu.\n");
    }
    else if (resp.code == 131) // CODE_ALREADY_SUBMITTED
    {
        ui_show_info("Exam already submitted.");
        display_exam_result(resp.message);

        // Return to menu
        client->state = CLIENT_AUTHENTICATED;
        memset(client->current_room, 0, sizeof(client->current_room));
    }
    else if (resp.code == 230) // CODE_TIME_EXPIRED
    {
        ui_show_error("Exam time expired. Your submitted answers will be graded by server.");
        client->state = CLIENT_AUTHENTICATED;
        memset(client->current_room, 0, sizeof(client->current_room));
    }
    else
    {
        char error[512];
        snprintf(error, sizeof(error), "[%d] %s", resp.code, resp.message);
        ui_show_error(error);

        // Return to menu
        client->state = CLIENT_AUTHENTICATED;
        memset(client->current_room, 0, sizeof(client->current_room));
    }
}

/**
 * @brief Display exam result from score|total format
 */
void display_exam_result(const char *result_str)
{
    if (!result_str)
        return;

    printf("\n========================================\n");
    printf("YOUR RESULT\n");
    printf("========================================\n");

    // Parse score|total
    char temp[256];
    strncpy(temp, result_str, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *score_str = strtok(temp, "|");
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
        printf("Result: %s\n", result_str);
    }

    printf("========================================\n");
}

/**
 * @brief Handle PRACTICE command - Start practice mode
 * User answers questions and auto-submits on timeout
 */
void handle_practice(Client *client)
{
    char num_questions_str[16];
    char time_minutes_str[16];

    printf("\n=== PRACTICE MODE ===\n");
    ui_get_input("Number of questions (5-50): ", num_questions_str, sizeof(num_questions_str));
    ui_get_input("Time limit (minutes, 1-60): ", time_minutes_str, sizeof(time_minutes_str));

    int num_questions = atoi(num_questions_str);
    int time_minutes = atoi(time_minutes_str);

    if (num_questions < 5 || num_questions > 50)
    {
        ui_show_error("Number of questions must be between 5 and 50");
        return;
    }

    if (time_minutes < 1 || time_minutes > 60)
    {
        ui_show_error("Time limit must be between 1 and 60 minutes");
        return;
    }

    // Send PRACTICE command
    const char *params[] = {num_questions_str, time_minutes_str};
    if (client_create_send_command(client, "PRACTICE", params, 2) < 0)
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

    if (resp.code == 140) // CODE_DATA
    {
        ui_show_success("Practice questions received!");

        // Parse JSON to extract practice_id and time_limit
        const char *practice_id_start = strstr(resp.data, "\"practice_id\":\"");
        if (!practice_id_start)
        {
            ui_show_error("Failed to parse practice_id");
            free(resp.data);
            return;
        }
        practice_id_start += 15;
        const char *practice_id_end = strstr(practice_id_start, "\"");
        size_t practice_id_len = practice_id_end - practice_id_start;
        strncpy(client->practice_id, practice_id_start, practice_id_len);
        client->practice_id[practice_id_len] = '\0';

        // Extract time_limit
        const char *time_limit_start = strstr(resp.data, "\"time_limit\":");
        int time_limit = 0;
        if (time_limit_start)
            time_limit = atoi(time_limit_start + 13);

        // Parse question_ids
        int total_questions = 0;
        int question_ids[100];
        memset(question_ids, 0, sizeof(question_ids));

        const char *search = resp.data;
        while ((search = strstr(search, "\"question_id\":")) != NULL && total_questions < 100)
        {
            question_ids[total_questions] = atoi(search + 14);
            total_questions++;
            search++;
        }

        printf("\n========================================\n");
        printf("PRACTICE MODE (%d questions, %d minutes)\n", total_questions, time_limit);
        printf("Practice ID: %s\n", client->practice_id);
        printf("========================================\n");

        // Display questions
        search = resp.data;
        int q_num = 1;
        while ((search = strstr(search, "\"content\":\"")) != NULL && q_num <= total_questions)
        {
            const char *content_start = search + 11;
            const char *content_end = strstr(content_start, "\"");

            if (content_end)
            {
                printf("\nQuestion %d: ", q_num);
                fwrite(content_start, 1, content_end - content_start, stdout);
                printf("\n");

                // Find and display options (already formatted by server)
                const char *options_start = strstr(content_end, "\"options\":[");
                if (options_start)
                {
                    options_start += 11;
                    const char *option_end = strstr(options_start, "]");

                    if (option_end)
                    {
                        const char *opt_ptr = options_start;
                        while (opt_ptr < option_end)
                        {
                            const char *opt_start = strstr(opt_ptr, "\"");
                            if (!opt_start || opt_start >= option_end)
                                break;
                            opt_start++;

                            const char *opt_close = strstr(opt_start, "\"");
                            if (!opt_close || opt_close >= option_end)
                                break;

                            printf("  ");
                            fwrite(opt_start, 1, opt_close - opt_start, stdout);
                            printf("\n");

                            opt_ptr = opt_close + 1;
                        }
                    }
                }

                q_num++;
                search = content_end + 1;
            }
            else
            {
                break;
            }
        }

        printf("========================================\n");
        printf("\nStarting practice...\n");

        // Update state
        client->state = CLIENT_IN_PRACTICE;

        // Collect answers
        char answers[512] = "";
        for (int i = 0; i < total_questions; i++)
        {
            char choice;
            int valid = 0;

            while (!valid)
            {
                printf("Answer for question %d (A/B/C/D): ", i + 1);
                if (scanf(" %c", &choice) != 1)
                {
                    ui_show_error("Failed to read input");
                    getchar();
                    continue;
                }
                getchar();

                if (choice >= 'a' && choice <= 'z')
                    choice -= 32;

                if (choice >= 'A' && choice <= 'D')
                    valid = 1;
                else
                    printf("Invalid! Please use A, B, C, or D.\n");
            }

            if (i > 0)
                strcat(answers, ",");

            char answer_str[2] = {choice, '\0'};
            strcat(answers, answer_str);

            printf("✓ Answer %d saved\n", i + 1);
        }

        printf("\n========================================\n");
        printf("All answers collected!\n");
        printf("========================================\n");

        free(resp.data);

        // Submit practice automatically
        printf("\nSubmitting practice...\n");
        handle_submit_practice(client, answers);
    }
    else
    {
        char error[512];
        snprintf(error, sizeof(error), "[%d] %s", resp.code, resp.message);
        ui_show_error(error);
    }
}

/**
 * @brief Handle SUBMIT_PRACTICE command
 */
void handle_submit_practice(Client *client, const char *answers)
{
    printf("\n=== SUBMIT PRACTICE ===\n");

    if (strlen(client->practice_id) == 0)
    {
        ui_show_error("No active practice session");
        return;
    }

    ui_show_info("Submitting practice...");

    // Send SUBMIT_PRACTICE command
    const char *params[] = {client->practice_id, answers};
    if (client_create_send_command(client, "SUBMIT_PRACTICE", params, 2) < 0)
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

    if (resp.code == 141) // CODE_PRACTICE_RESULT
    {
        printf("\n========================================\n");
        printf("YOUR PRACTICE RESULT\n");
        printf("========================================\n");

        // Parse score|total|TIMEOUT?
        char temp[256];
        strncpy(temp, resp.message, sizeof(temp) - 1);
        temp[sizeof(temp) - 1] = '\0';

        char *score_str = strtok(temp, "|");
        char *total_str = strtok(NULL, "|");
        char *timeout_flag = strtok(NULL, "|");

        if (score_str && total_str)
        {
            int score = atoi(score_str);
            int total = atoi(total_str);
            double percentage = (total > 0) ? (score * 100.0 / total) : 0.0;

            // Check if timeout occurred
            if (timeout_flag && strcmp(timeout_flag, "TIMEOUT") == 0)
            {
                printf("⏱️  TIME EXPIRED!\n");
                printf("Your answers have been auto-submitted.\n\n");
            }
            else
            {
                ui_show_success("Practice submitted successfully!");
            }

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

        // Return to menu
        client->state = CLIENT_AUTHENTICATED;
        memset(client->practice_id, 0, sizeof(client->practice_id));
        printf("\nYou have been returned to the main menu.\n");
    }
    else if (resp.code == 230) // CODE_TIME_EXPIRED
    {
        ui_show_error("Practice time expired. Your submitted answers will be graded by server.");
        client->state = CLIENT_AUTHENTICATED;
        memset(client->practice_id, 0, sizeof(client->practice_id));
    }
    else
    {
        char error[512];
        snprintf(error, sizeof(error), "[%d] %s", resp.code, resp.message);
        ui_show_error(error);
        client->state = CLIENT_AUTHENTICATED;
        memset(client->practice_id, 0, sizeof(client->practice_id));
    }
}
