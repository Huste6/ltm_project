#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// ===============================================
// PROTOCOL DEFINITIONS - Mã lỗi và response code
// ===============================================

// Authentication & Session Codes
#define CODE_CREATED 100   // Đăng ký thành công
#define CODE_LOGIN_OK 110  // Đăng nhập thành công
#define CODE_LOGOUT_OK 132 // Đăng xuất thành công

// Room Management Codes
#define CODE_ROOM_CREATED 120  // Tạo phòng thành công
#define CODE_ROOMS_DATA 121    // Danh sách phòng
#define CODE_ROOM_JOIN_OK 122  // Vào phòng thành công
#define CODE_ROOM_LEAVE_OK 123 // Rời phòng thành công
#define CODE_START_OK 125      // Bắt đầu thi
#define CODE_RESULT_DATA 127   // Dữ liệu kết quả

// Exam & Submit Codes
#define CODE_SUBMIT_OK 130         // Nộp bài lần đầu
#define CODE_ALREADY_SUBMITTED 131 // Đã nộp rồi
#define CODE_ANSWER_SAVED 160      // Lưu đáp án thành công

// Data Transfer Codes
#define CODE_DATA 140            // Dữ liệu luyện tập
#define CODE_PRACTICE_RESULT 141 // Kết quả luyện tập
#define CODE_EXAM_DATA 150       // Dữ liệu đề thi

// Ping/Pong
#define CODE_PONG 200   // Response to PING
#define CODE_WHOAMI 201 // Trả thông tin user

// Authentication Errors
#define CODE_ACCOUNT_LOCKED 211    // Tài khoản bị khóa
#define CODE_ACCOUNT_NOT_FOUND 212 // Không tìm thấy user
#define CODE_ALREADY_LOGGED 213    // Đã đăng nhập nơi khác
#define CODE_WRONG_PASSWORD 214    // Sai mật khẩu
#define CODE_NOT_LOGGED 221        // Chưa đăng nhập
#define CODE_SESSION_EXPIRED 222   // Session hết hạn

// Room Errors
#define CODE_ROOM_NOT_FOUND 223   // Không tìm thấy phòng
#define CODE_ROOM_IN_PROGRESS 224 // Phòng đã bắt đầu
#define CODE_ROOM_FINISHED 225    // Phòng đã kết thúc
#define CODE_NOT_CREATOR 226      // Không phải người tạo
#define CODE_NOT_IN_ROOM 227      // Không ở trong phòng
#define CODE_ROOM_FULL 228        // Phòng đầy
#define CODE_EXAM_STARTED 124     // Exam bắt đầu
#define CODE_EXAM_FINISHED 125    // Exam kết thúc
#define CODE_NOT_ALLOWED 229      // Không có quyền
#define CODE_INVALID_STATE 231    // Trạng thái không hợp lệ

// Submit Errors
#define CODE_TIME_EXPIRED 230 // Hết giờ

// General Errors
#define CODE_BAD_COMMAND 300    // Lệnh không hợp lệ
#define CODE_SYNTAX_ERROR 301   // Lỗi cú pháp
#define CODE_INVALID_PARAMS 302 // Tham số không hợp lệ

// Registration Errors
#define CODE_USERNAME_EXISTS 401  // Username đã tồn tại
#define CODE_INVALID_USERNAME 402 // Username không hợp lệ
#define CODE_WEAK_PASSWORD 403    // Password yếu

// Server Errors
#define CODE_INTERNAL_ERROR 500 // Lỗi server

// ===============================================
// MESSAGE TYPES - Các loại message
// ===============================================

#define MSG_REGISTER "REGISTER"
#define MSG_LOGIN "LOGIN"
#define MSG_LOGOUT "LOGOUT"
#define MSG_PRACTICE "PRACTICE"
#define MSG_SUBMIT_PRACTICE "SUBMIT_PRACTICE"
#define MSG_CREATE_ROOM "CREATE_ROOM"
#define MSG_LIST_ROOMS "LIST_ROOMS"
#define MSG_JOIN_ROOM "JOIN_ROOM"
#define MSG_LEAVE_ROOM "LEAVE_ROOM"
#define MSG_START_EXAM "START_EXAM"
#define MSG_GET_EXAM "GET_EXAM"
#define MSG_SAVE_ANSWER "SAVE_ANSWER"
#define MSG_SUBMIT_EXAM "SUBMIT_EXAM"
#define MSG_VIEW_RESULT "VIEW_RESULT"
#define MSG_PING "PING"
#define MSG_WHOAMI "WHOAMI"

// ==========================================
// PROTOCOL CONSTANTS
// ==========================================

#define MAX_USERNAME_LEN 20
#define MAX_PASSWORD_LEN 50
#define MAX_ROOM_NAME_LEN 100
#define MAX_ROOM_ID_LEN 32
#define MAX_SESSION_ID_LEN 64
#define MAX_MESSAGE_LEN 8192
#define MAX_DATA_SIZE (1024 * 1024)
#define MAX_PARAMS 10
#define DELIMITER '\n'

// ==========================================
// STRUCTURES - Cấu trúc dữ liệu giao thức
// ==========================================

/**
 * @brief Cấu trúc message gửi/nhận
 * Format: COMMAND param1|param2|...\n
 * hoặc: CODE DATA length\n<data>
 */
typedef struct
{
    char command[50];     // Command name or code
    char params[10][256]; // Up to 10 parameters, each up to 256 chars
    int param_count;      // Number of parameters
    char *data;           // data payload
    size_t data_length;   // length of data payload
} Message;

/**
 * @brief Cấu trúc response từ server
 */
typedef struct
{
    int code;           // Response code
    char message[256];  // Message text
    char *data;         // data payload
    size_t data_length; // length of data payload
} Response;

// ==========================================
// FUNCTION PROTOTYPES
// ==========================================

/**
 * @brief Parse message từ buffer nhận được
 * @param buffer Buffer chứa message
 * @param msg Pointer đến Message struct để lưu kết quả
 * @return 0 nếu thành công, -1 nếu lỗi
 *
 * Format hỗ trợ:
 * 1. Control message: COMMAND param1|param2\n
 * 2. Data message: CODE DATA length\n<data>
 */
int parse_message(const char *buffer, Message *msg);

/**
 * @brief Tạo control message để gửi
 * @param command Tên command (REGISTER, LOGIN, etc.)
 * @param params Mảng tham số
 * @param param_count Số lượng tham số
 * @param buffer Buffer output
 * @param buffer_size Kích thước buffer
 * @return Số bytes đã ghi vào buffer
 *
 * Ví dụ: create_control_message("LOGIN", ["john", "pass123"], 2, buffer, size)
 * Output: "LOGIN john|pass123\n"
 */
int create_control_message(const char *command, const char **params, int param_count, char *buffer, size_t buffer_size);

/**
 * @brief Tạo data message với length prefixing
 * @param code Response code (140, 150, etc.)
 * @param data Dữ liệu (JSON, binary, etc.)
 * @param data_len Độ dài data
 * @param buffer Buffer output
 * @param buffer_size Kích thước buffer
 * @return Số bytes đã ghi vào buffer
 *
 * Format: "CODE DATA <length>\n<data>"
 * Ví dụ: "140 DATA 1234\n<1234 bytes>"
 */
int create_data_message(int code, const char *data, size_t data_len, char *buffer, size_t buffer_size);

/**
 * @brief Tạo response đơn giản (chỉ code + message)
 * @param code Response code
 * @param message Message text (optional)
 * @param buffer Buffer output
 * @param buffer_size Kích thước buffer
 * @return Số bytes đã ghi
 *
 * Format: "CODE MESSAGE\n"
 * Ví dụ: "110 LOGIN_OK sess_12345\n"
 */
int create_simple_response(int code, const char *message, char *buffer, size_t buffer_size);

/**
 * @brief Nhận đầy đủ n bytes từ socket (xử lý fragmentation)
 * @param sockfd Socket descriptor
 * @param buffer Buffer để lưu data
 * @param n Số bytes cần nhận
 * @return Số bytes thực tế nhận được, -1 nếu lỗi
 *
 * Hàm này đảm bảo nhận đủ n bytes, xử lý trường hợp
 * TCP stream bị phân mảnh
 */
int recv_full(int sockfd, char *buffer, size_t n);

/**
 * @brief Gửi đầy đủ n bytes qua socket
 * @param sockfd Socket descriptor
 * @param buffer Data cần gửi
 * @param n Số bytes cần gửi
 * @return Số bytes đã gửi, -1 nếu lỗi
 */
int send_full(int sockfd, const char *buffer, size_t n);

/**
 * @brief Nhận message với delimiter '\n'
 * @param sockfd Socket descriptor
 * @param buffer Buffer output
 * @param buffer_size Kích thước buffer
 * @return Số bytes nhận được, -1 nếu lỗi
 *
 * Đọc cho đến khi gặp '\n'
 */
int recv_line(int sockfd, char *buffer, size_t buffer_size);

/**
 * @brief Validate username format
 * @param username Username cần kiểm tra
 * @return 1 nếu hợp lệ, 0 nếu không
 *
 * Rules:
 * - Độ dài: 3-20 ký tự
 * - Chỉ chứa: a-z, A-Z, 0-9, underscore
 */
int validate_username(const char *username);

/**
 * @brief Validate password strength
 * @param password Password cần kiểm tra
 * @return 1 nếu hợp lệ, 0 nếu không
 *
 * Rules:
 * - Tối thiểu 8 ký tự
 * - Có ít nhất 1 chữ hoa
 * - Có ít nhất 1 chữ thường
 * - Có ít nhất 1 số
 */
int validate_password(const char *password);

/**
 * @brief Free memory của Message struct
 * @param msg Message cần free
 */
void free_message(Message *msg);

/**
 * @brief Free memory của Response struct
 * @param resp Response cần free
 */
void free_response(Response *resp);

/**
 * @brief Get response code description
 * @param code Response code
 * @return String mô tả code
 */
const char *get_code_description(int code);

#endif // PROTOCOL_H