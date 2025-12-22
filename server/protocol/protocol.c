#include "protocol.h"
#include <ctype.h>
#include <sys/socket.h>
#include <unistd.h>

int recv_full(int sockfd, char *buffer, size_t n)
{
    size_t total_received = 0;
    while (total_received < n)
    {
        ssize_t bytes_received = recv(sockfd, buffer + total_received, n - total_received, 0);
        if (bytes_received <= 0)
        {
            return -1;
        }
        total_received += bytes_received;
    }
    return total_received;
}

int send_full(int sockfd, const char *buffer, size_t n)
{
    size_t total_sent = 0;
    while (total_sent < n)
    {
        ssize_t bytes_sent = send(sockfd, buffer + total_sent, n - total_sent, 0);
        if (bytes_sent <= 0)
        {
            return -1;
        }
        total_sent += bytes_sent;
    }
    return total_sent;
}

int recv_line(int sockfd, char *buffer, size_t buffer_size)
{
    size_t total_received = 0;
    while (total_received < buffer_size - 1)
    {
        char ch;
        ssize_t bytes_received = recv(sockfd, &ch, 1, 0);
        if (bytes_received <= 0)
        {
            return -1;
        }
        buffer[total_received++] = ch;
        if (ch == '\n')
        {
            break;
        }
    }
    buffer[total_received] = '\0';
    return total_received;
}

//  * 1. Control message: COMMAND param1|param2\n
// => Msg struct
// msg->command = COMMAND
// msg->params = [param1, param2]
// msg->param_count = 2
// msg->data = NULL
// msg->data_length = 0
//  * 2. Data message: CODE DATA length\n<data>
// => Msg struct
// msg->command = CODE (as string)
// msg->params = []
// msg->param_count = 0
// msg->data = <data>
// msg->data_length = length
int parse_message(const char *buffer, Message *msg)
{
    memset(msg, 0, sizeof(Message));

    char tmp[MAX_MESSAGE_LEN];
    strncpy(tmp, buffer, MAX_MESSAGE_LEN - 1);
    tmp[MAX_MESSAGE_LEN - 1] = '\0';

    // find first newline
    char *newline = strchr(tmp, '\n');
    if (!newline)
        return -1;

    // check if this is a data message containing "DATA"
    char *data_keyword = strstr(tmp, " DATA ");
    if (data_keyword)
    {
        // Format: "CODE DATA length\n<data>"
        // Example: "140 DATA 1234\n<1234 bytes>"

        *data_keyword = '\0';

        // Parse: CODE DATA length
        char *token = strtok(tmp, " "); // token = "CODE"
        if (!token)
            return -1;
        strncpy(msg->command, token, sizeof(msg->command) - 1); // CODE

        token = strtok(NULL, " "); // token = "DATA"
        if (!token || strcmp(token, "DATA") != 0)
            return -1;

        token = strtok(NULL, " "); // token = "length"
        if (!token)
            return -1;

        msg->data_length = (size_t)atoll(token);

        // The actual data starts after the newline
        const char *data_start = newline + 1;
        if (msg->data_length > 0)
        {
            msg->data = (char *)malloc(msg->data_length);
            if (!msg->data)
                return -1;
            memcpy(msg->data, data_start, msg->data_length);
        }
        msg->param_count = 0;
    }
    else
    {
        // Format: "COMMAND param1|param2|param3\n"
        // Example: "REGISTER john123|Password123\n"

        *newline = '\0';

        // Parse: COMMAND and params
        char *space = strchr(tmp, ' ');
        if (space)
        {
            *space = '\0';
            strncpy(msg->command, tmp, sizeof(msg->command) - 1);

            char *params_str = space + 1;
            char *param_token = strtok(params_str, "|");
            while (param_token && msg->param_count < MAX_PARAMS)
            {
                strncpy(msg->params[msg->param_count], param_token, 255);
                msg->params[msg->param_count][255] = '\0';
                msg->param_count++;
                param_token = strtok(NULL, "|");
            }
        }
        else
        {
            // No params, only command (e.g., "PING\n", "LOGOUT\n")
            strncpy(msg->command, tmp, sizeof(msg->command) - 1);
            msg->param_count = 0;
        }

        msg->data = NULL;
        msg->data_length = 0;
    }
    return 0;
}

// create_control_message("LOGIN", ["john", "pass123"], 2, buffer, size) -> "LOGIN john|pass123\n"
int create_control_message(const char *command, const char **params, int param_count, char *buffer, size_t buffer_size)
{
    size_t offset = snprintf(buffer, buffer_size, "%s", command); // buffer = "COMMAND"

    // offset < buffer_size - 2 to leave space for '\n' and '\0'
    for (int i = 0; i < param_count && offset < buffer_size - 2; i++)
    {
        int written = snprintf(buffer + offset, buffer_size - offset, "%s%s", i == 0 ? " " : "|", params[i]);
        if (written < 0)
            break;
        offset += written;
    }
    if (offset < buffer_size - 1)
    {
        buffer[offset++] = '\n';
        buffer[offset] = '\0';
    }
    return (int)offset;
}

// Format: "CODE DATA <length>\n<data>"
// Ví dụ: "140 DATA 1234\n<1234 bytes>"
int create_data_message(int code, const char *data, size_t data_len, char *buffer, size_t buffer_size)
{
    int header_len = snprintf(buffer, buffer_size, "%d DATA %zu\n", code, data_len);

    if (header_len + data_len > buffer_size)
        return -1;

    memcpy(buffer + header_len, data, data_len);
    return header_len + data_len;
}

// Format: "CODE MESSAGE\n"
// Ví dụ: "110 LOGIN_OK sess_12345\n"
int create_simple_response(int code, const char *message, char *buffer, size_t buffer_size)
{
    if (message && strlen(message) > 0)
    {
        return snprintf(buffer, buffer_size, "%d %s\n", code, message);
    }
    else
    {
        return snprintf(buffer, buffer_size, "%d\n", code);
    }
}

int validate_username(const char *username)
{
    size_t len = strlen(username);
    if (len < 3 || len > 20)
        return 0;

    for (size_t i = 0; i < len; i++)
    {
        if (!isalnum(username[i]) && username[i] != '_')
        {
            return 0;
        }
    }
    return 1;
}

int validate_password(const char *password)
{
    size_t len = strlen(password);
    if (len < 8)
        return 0;

    int has_upper = 0, has_lower = 0, has_digit = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (isupper(password[i]))
            has_upper = 1;
        if (islower(password[i]))
            has_lower = 1;
        if (isdigit(password[i]))
            has_digit = 1;
    }

    return has_upper && has_lower && has_digit;
}

void free_message(Message *msg)
{
    if (msg && msg->data)
    {
        free(msg->data);
        msg->data = NULL;
    }
}

void free_response(Response *resp)
{
    if (resp && resp->data)
    {
        free(resp->data);
        resp->data = NULL;
    }
}

const char *get_code_description(int code)
{
    switch (code)
    {
    case CODE_CREATED:
        return "CREATED";
    case CODE_LOGIN_OK:
        return "LOGIN_OK";
    case CODE_LOGOUT_OK:
        return "LOGOUT_OK";
    case CODE_ROOM_CREATED:
        return "ROOM_CREATED";
    case CODE_ROOMS_DATA:
        return "DATA";
    case CODE_ROOM_JOIN_OK:
        return "ROOM_JOIN_OK";
    case CODE_ROOM_LEAVE_OK:
        return "ROOM_LEAVE_OK";
    case CODE_START_OK:
        return "START_OK";
    case CODE_RESULT_DATA:
        return "DATA";
    case CODE_SUBMIT_OK:
        return "SUBMIT_OK";
    case CODE_ALREADY_SUBMITTED:
        return "ALREADY_SUBMITTED";
    case CODE_DATA:
        return "DATA";
    case CODE_PRACTICE_RESULT:
        return "PRACTICE_RESULT";
    case CODE_EXAM_DATA:
        return "DATA";
    case CODE_NOT_LOGGED:
        return "NOT_LOGGED";
    case CODE_SESSION_EXPIRED:
        return "SESSION_EXPIRED";
    case CODE_ROOM_NOT_FOUND:
        return "ROOM_NOT_FOUND";
    case CODE_ROOM_IN_PROGRESS:
        return "ROOM_ALREADY_STARTED";
    case CODE_ROOM_FINISHED:
        return "ROOM_FINISHED";
    case CODE_NOT_CREATOR:
        return "NOT_CREATOR";
    case CODE_NOT_IN_ROOM:
        return "NOT_IN_ROOM";
    case CODE_ROOM_FULL:
        return "ROOM_FULL";
    case CODE_TIME_EXPIRED:
        return "TIME_EXPIRED";
    case CODE_BAD_COMMAND:
        return "BAD_COMMAND";
    case CODE_SYNTAX_ERROR:
        return "SYNTAX_ERROR";
    case CODE_INVALID_PARAMS:
        return "INVALID_PARAMS";
    case CODE_USERNAME_EXISTS:
        return "USERNAME_EXISTS";
    case CODE_INVALID_USERNAME:
        return "INVALID_USERNAME";
    case CODE_WEAK_PASSWORD:
        return "WEAK_PASSWORD";
    case CODE_INTERNAL_ERROR:
        return "INTERNAL_ERROR";
    default:
        return "UNKNOWN";
    }
}
